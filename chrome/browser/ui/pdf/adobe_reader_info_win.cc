// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/adobe_reader_info_win.h"

#include <shlwapi.h>

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_version_info.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/plugin_service.h"

namespace {

// Hardcoded value for the secure version of Acrobat Reader.
const char kSecureVersion[] = "11.0.7.79";

const char kAdobeReaderIdentifier[] = "adobe-reader";
const char kPdfMimeType[] = "application/pdf";
const base::char16 kRegistryAcrobat[] = L"Acrobat.exe";
const base::char16 kRegistryAcrobatReader[] = L"AcroRd32.exe";
const base::char16 kRegistryApps[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths";
const base::char16 kRegistryPath[] = L"Path";

// Gets the installed path for a registered app.
base::FilePath GetInstalledPath(const base::char16* app) {
  base::string16 reg_path(kRegistryApps);
  reg_path.append(L"\\");
  reg_path.append(app);

  base::FilePath filepath;
  base::win::RegKey hkcu_key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  base::string16 path;
  // As of Win7 AppPaths can also be registered in HKCU: http://goo.gl/UgFOf.
  if (base::win::GetVersion() >= base::win::VERSION_WIN7 &&
      hkcu_key.ReadValue(kRegistryPath, &path) == ERROR_SUCCESS) {
    filepath = base::FilePath(path);
  } else {
    base::win::RegKey hklm_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
    if (hklm_key.ReadValue(kRegistryPath, &path) == ERROR_SUCCESS) {
      filepath = base::FilePath(path);
    }
  }
  return filepath.Append(app);
}

bool IsPdfMimeType(const content::WebPluginMimeType& plugin_mime_type) {
  return plugin_mime_type.mime_type == kPdfMimeType;
}

AdobeReaderPluginInfo GetReaderPlugin(
    Profile* profile,
    const std::vector<content::WebPluginInfo>& plugins) {
  AdobeReaderPluginInfo reader_info;
  reader_info.is_installed = false;
  reader_info.is_enabled = false;
  reader_info.is_secure = false;

  PluginFinder* plugin_finder = PluginFinder::GetInstance();
  for (size_t i = 0; i < plugins.size(); ++i) {
    const content::WebPluginInfo& plugin = plugins[i];
    if (plugin.is_pepper_plugin())
      continue;
    if (std::find_if(plugin.mime_types.begin(), plugin.mime_types.end(),
                     IsPdfMimeType) == plugin.mime_types.end())
      continue;
    scoped_ptr<PluginMetadata> plugin_metadata(
        plugin_finder->GetPluginMetadata(plugins[i]));
    if (plugin_metadata->identifier() != kAdobeReaderIdentifier)
      continue;

    reader_info.is_installed = true;

    if (profile) {
      PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile);
      PluginPrefs::PolicyStatus plugin_status =
          plugin_prefs->PolicyStatusForPlugin(plugin_metadata->name());
      reader_info.is_enabled = plugin_status != PluginPrefs::POLICY_DISABLED;
    }

    PluginMetadata::SecurityStatus security_status =
        plugin_metadata->GetSecurityStatus(plugins[i]);
    reader_info.is_secure =
        security_status == PluginMetadata::SECURITY_STATUS_UP_TO_DATE;

    reader_info.plugin_info = plugins[i];
    break;
  }
  return reader_info;
}

void OnGotPluginInfo(Profile* profile,
                     const GetAdobeReaderPluginInfoCallback& callback,
                     const std::vector<content::WebPluginInfo>& plugins) {
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    profile = NULL;
  callback.Run(GetReaderPlugin(profile, plugins));
}

bool IsAdobeReaderDefaultPDFViewerInternal(base::FilePath* path) {
  base::char16 app_cmd_buf[MAX_PATH];
  DWORD app_cmd_buf_len = MAX_PATH;
  HRESULT hr = AssocQueryString(ASSOCF_NONE, ASSOCSTR_COMMAND, L".pdf", L"open",
                                app_cmd_buf, &app_cmd_buf_len);
  if (FAILED(hr))
    return false;

  // Looks for the install paths for Acrobat / Reader.
  base::FilePath install_path = GetInstalledPath(kRegistryAcrobatReader);
  if (install_path.empty())
    install_path = GetInstalledPath(kRegistryAcrobat);
  if (install_path.empty())
    return false;

  base::string16 app_cmd(app_cmd_buf);
  bool found = app_cmd.find(install_path.value()) != base::string16::npos;
  if (found && path)
    *path = install_path;
  return found;
}

}  // namespace

void GetAdobeReaderPluginInfoAsync(
    Profile* profile,
    const GetAdobeReaderPluginInfoCallback& callback) {
  DCHECK(!callback.is_null());
  content::PluginService::GetInstance()->GetPlugins(
      base::Bind(&OnGotPluginInfo, profile, callback));
}

bool GetAdobeReaderPluginInfo(Profile* profile,
                              AdobeReaderPluginInfo* reader_info) {
  DCHECK(reader_info);
  std::vector<content::WebPluginInfo> plugins;
  bool up_to_date = content::PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), kPdfMimeType, false, &plugins, NULL);
  *reader_info = GetReaderPlugin(profile, plugins);
  return up_to_date;
}

bool IsAdobeReaderDefaultPDFViewer() {
  return IsAdobeReaderDefaultPDFViewerInternal(NULL);
}

bool IsAdobeReaderUpToDate() {
  base::FilePath install_path;
  bool is_default = IsAdobeReaderDefaultPDFViewerInternal(&install_path);
  if (!is_default)
    return false;

  scoped_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfo(install_path));
  if (!file_version_info)
    return false;

  std::string reader_version =
      base::UTF16ToUTF8(file_version_info->product_version());
  // Convert 1.2.03.45 to 1.2.3.45 so base::Version considers it as valid.
  for (int i = 1; i <= 9; ++i) {
    std::string from = base::StringPrintf(".0%d", i);
    std::string to = base::StringPrintf(".%d", i);
    ReplaceSubstringsAfterOffset(&reader_version, 0, from, to);
  }
  base::Version file_version(reader_version);
  return file_version.IsValid() && !file_version.IsOlderThan(kSecureVersion);
}
