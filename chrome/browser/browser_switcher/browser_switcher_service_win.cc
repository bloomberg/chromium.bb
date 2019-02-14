// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service_win.h"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "chrome/browser/profiles/profile.h"

namespace browser_switcher {

namespace {

const wchar_t kIeSiteListKey[] =
    L"SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Main\\EnterpriseMode";
const wchar_t kIeSiteListValue[] = L"SiteList";

const int kCurrentFileVersion = 1;

base::FilePath GetCacheDir() {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path))
    return path;
  path = path.AppendASCII("Google");
  path = path.AppendASCII("BrowserSwitcher");
  return path;
}

// Serialize prefs to a string for writing to cache.dat.
std::string SerializeCacheFile(const BrowserSwitcherPrefs& prefs) {
  std::ostringstream buffer;

  buffer << kCurrentFileVersion << std::endl;

  buffer << prefs.GetAlternativeBrowserPath() << std::endl;
  buffer << base::JoinString(prefs.GetAlternativeBrowserParameters(), " ")
         << std::endl;

  // TODO(nicolaso): Use GetChromePath() and GetChromeParameters once the
  // policies are implemented. For now, those are just ${chrome} with no
  // arguments, to ensure the BHO works correctly.
  buffer << "${chrome}" << std::endl;
  buffer << base::JoinString(std::vector<std::string>(), " ") << std::endl;

  const auto& rules = prefs.GetRules();
  buffer << rules.sitelist.size() << std::endl;
  if (!rules.sitelist.empty())
    buffer << base::JoinString(rules.sitelist, "\n") << std::endl;

  buffer << rules.greylist.size() << std::endl;
  if (!rules.greylist.empty())
    buffer << base::JoinString(rules.greylist, "\n") << std::endl;

  return buffer.str();
}

// Does the I/O for the |SavePrefsToFile()|. Should be run in a worker thread to
// avoid blocking the main thread.
void DoSavePrefsToFile(std::string data) {
  // Ensure the directory exists.
  base::FilePath dir = GetCacheDir();

  if (dir.empty())
    return;

  bool success = base::CreateDirectory(dir);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.CacheFile.MkDirSuccess", success);
  if (!success) {
    LOG(ERROR) << "Could not create directory: " << dir.LossyDisplayName();
    return;
  }

  base::FilePath tmp_path;
  success = base::CreateTemporaryFileInDir(dir, &tmp_path);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.CacheFile.MkTempSuccess", success);
  if (!success) {
    LOG(ERROR) << "Could not open file for writing: "
               << tmp_path.LossyDisplayName();
    return;
  }

  base::WriteFile(tmp_path, data.c_str(), data.size());

  base::FilePath dest_path = dir.AppendASCII("cache.dat");
  success = base::Move(tmp_path, dest_path);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.CacheFile.MoveSuccess", success);
}

// URL to fetch the IEEM sitelist from. Only used for testing.
base::Optional<std::string>* IeemSitelistUrlForTesting() {
  static base::NoDestructor<base::Optional<std::string>>
      ieem_sitelist_url_for_testing;
  return ieem_sitelist_url_for_testing.get();
}

void DoRemovePrefsFile() {
  base::FilePath dir = GetCacheDir();

  if (dir.empty())
    return;

  // Ignore errors while deleting.
  base::FilePath dest_path = dir.AppendASCII("cache.dat");
  base::DeleteFile(dest_path, false);
}

}  // namespace

BrowserSwitcherServiceWin::BrowserSwitcherServiceWin(Profile* profile)
    : BrowserSwitcherService(profile), weak_ptr_factory_(this) {
  if (prefs_.UseIeSitelist()) {
    GURL sitelist_url = GetIeemSitelistUrl();
    if (sitelist_url.is_valid()) {
      ieem_downloader_ = std::make_unique<XmlDownloader>(
          profile, std::move(sitelist_url), fetch_delay_,
          base::BindOnce(&BrowserSwitcherServiceWin::OnIeemSitelistParsed,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  if (prefs_.IsEnabled())
    SavePrefsToFile();
  else
    DeletePrefsFile();

  prefs_subscription_ = prefs_.RegisterPrefsChangedCallback(base::BindRepeating(
      &BrowserSwitcherServiceWin::OnBrowserSwitcherPrefsChanged,
      base::Unretained(this)));
}

BrowserSwitcherServiceWin::~BrowserSwitcherServiceWin() = default;

void BrowserSwitcherServiceWin::OnBrowserSwitcherPrefsChanged(
    BrowserSwitcherPrefs* prefs) {
  if (prefs_.IsEnabled())
    SavePrefsToFile();
  else
    DeletePrefsFile();
}

// static
void BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(
    const std::string& spec) {
  *IeemSitelistUrlForTesting() = spec;
}

// static
GURL BrowserSwitcherServiceWin::GetIeemSitelistUrl() {
  if (*IeemSitelistUrlForTesting() != base::nullopt)
    return GURL((*IeemSitelistUrlForTesting()).value());

  base::win::RegKey key;
  if (ERROR_SUCCESS != key.Open(HKEY_LOCAL_MACHINE, kIeSiteListKey, KEY_READ) &&
      ERROR_SUCCESS != key.Open(HKEY_CURRENT_USER, kIeSiteListKey, KEY_READ)) {
    return GURL();
  }
  std::wstring url_string;
  if (ERROR_SUCCESS != key.ReadValue(kIeSiteListValue, &url_string))
    return GURL();
  return GURL(base::UTF16ToUTF8(url_string));
}

void BrowserSwitcherServiceWin::OnIeemSitelistParsed(ParsedXml xml) {
  if (xml.error) {
    LOG(ERROR) << "Unable to parse IEEM SiteList: " << *xml.error;
  } else {
    VLOG(2) << "Done parsing IEEM SiteList. "
            << "Applying rules to future navigations.";
    sitelist()->SetIeemSitelist(std::move(xml));
  }
  ieem_downloader_.reset();
}

void BrowserSwitcherServiceWin::SavePrefsToFile() const {
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&DoSavePrefsToFile, SerializeCacheFile(prefs_)));
}

void BrowserSwitcherServiceWin::DeletePrefsFile() const {
  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                            base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
                           base::BindOnce(&DoRemovePrefsFile));
}

}  // namespace browser_switcher
