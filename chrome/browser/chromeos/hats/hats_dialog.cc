// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/hats/hats_dialog.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/system/version_loader.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

namespace {

// Default width/height ratio of screen size.
const int kDefaultWidth = 400;
const int kDefaultHeight = 420;
// Site ID for HaTS survey.
constexpr char kSiteID[] = "cs5lsagwwbho7l5cbbdniso22e";
constexpr char kGooglerSiteID[] = "z56p2hjy7pegxh3gmmur4qlwha";

constexpr char kScriptSrcReplacementToken[] = "$SCRIPT_SRC";
constexpr char kDoneButtonLabelReplacementToken[] = "$DONE_BUTTON_LABEL";
// Base URL to fetch the google consumer survey script.
constexpr char kBaseFormatUrl[] =
    "https://www.google.com/insights/consumersurveys/"
    "async_survey?site=%s&force_https=1&sc=%s";
// Keyword used to join the separate device info elements into a single string
// to be used as site context.
const char kDeviceInfoStopKeyword[] = "STOP";
const char kDefaultProfileLocale[] = "en-US";
enum class DeviceInfoKey : unsigned int {
  BROWSER = 0,
  PLATFORM,
  FIRMWARE,
  LOCALE,
};

// Returns the local HaTS HTML file as a string with the correct Hats script
// URL.
std::string LoadLocalHtmlAsString(std::string site_id,
                                  std::string site_context) {
  std::string html_data;
  ResourceBundle::GetSharedInstance()
      .GetRawDataResource(IDR_HATS_HTML)
      .CopyToString(&html_data);

  size_t pos = html_data.find(kScriptSrcReplacementToken);
  html_data.replace(pos, strlen(kScriptSrcReplacementToken),
                    base::StringPrintf(kBaseFormatUrl, site_id.c_str(),
                                       site_context.c_str()));

  pos = html_data.find(kDoneButtonLabelReplacementToken);
  html_data.replace(pos, strlen(kDoneButtonLabelReplacementToken),
                    l10n_util::GetStringUTF8(IDS_HATS_DONE_BUTTON_LABEL));
  return html_data;
}

// Maps the given DeviceInfoKey |key| enum to the corresponding string value
// that can be used as a key when creating a URL parameter.
const std::string KeyEnumToString(DeviceInfoKey key) {
  switch (key) {
    case DeviceInfoKey::BROWSER:
      return "browser";
    case DeviceInfoKey::PLATFORM:
      return "platform";
    case DeviceInfoKey::FIRMWARE:
      return "firmware";
    case DeviceInfoKey::LOCALE:
      return "locale";
    default:
      NOTREACHED();
      return std::string();
  }
}

// Must be run on a blocking thread pool.
// Gathers the browser version info, firmware info and platform info and returns
// them in a single encoded string, the format of which is defined below.
// Currently the format is "<key><value>STOP<key><value>STOP<key><value>" where
// 'STOP' is used as a token to identify the end of a key value pair. This is
// done since GCS only allows the use of alphanumeric characters to be passed as
// a site context.
std::string GetFormattedSiteContext(std::string user_locale,
                                    base::StringPiece join_keyword) {
  std::vector<std::string> pairs;
  pairs.push_back(KeyEnumToString(DeviceInfoKey::BROWSER) +
                  version_info::GetVersionNumber());

  pairs.push_back(KeyEnumToString(DeviceInfoKey::PLATFORM) +
                  version_loader::GetVersion(version_loader::VERSION_FULL));

  pairs.push_back(KeyEnumToString(DeviceInfoKey::FIRMWARE) +
                  version_loader::GetFirmware());

  pairs.push_back(KeyEnumToString(DeviceInfoKey::LOCALE) + user_locale);

  return base::JoinString(pairs, join_keyword);
}

}  // namespace

// static
void HatsDialog::CreateAndShow(bool is_google_account) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::string user_locale =
      profile->GetPrefs()->GetString(prefs::kApplicationLocale);
  if (!user_locale.length())
    user_locale = kDefaultProfileLocale;

  std::unique_ptr<HatsDialog> hats_dialog(new HatsDialog);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&GetFormattedSiteContext, user_locale, kDeviceInfoStopKeyword),
      base::Bind(&HatsDialog::Show, base::Passed(&hats_dialog),
                 is_google_account ? kGooglerSiteID : kSiteID));
}

// static
void HatsDialog::Show(std::unique_ptr<HatsDialog> hats_dialog,
                      std::string site_id,
                      std::string site_context) {
  // Load and set the html data that needs to be displayed in the dialog.
  hats_dialog->html_data_ = LoadLocalHtmlAsString(site_id, site_context);

  chrome::ShowWebDialog(
      nullptr, ProfileManager::GetActiveUserProfile()->GetOffTheRecordProfile(),
      hats_dialog.release());
}

HatsDialog::HatsDialog() {}

HatsDialog::~HatsDialog() {}

ui::ModalType HatsDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 HatsDialog::GetDialogTitle() const {
  return base::string16();
}

GURL HatsDialog::GetDialogContentURL() const {
  return GURL("data:text/html;charset=utf-8," + html_data_);
}

void HatsDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {}

void HatsDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

bool HatsDialog::CanResizeDialog() const {
  return false;
}

std::string HatsDialog::GetDialogArgs() const {
  return std::string();
}

void HatsDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void HatsDialog::OnCloseContents(WebContents* source, bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool HatsDialog::ShouldShowDialogTitle() const {
  return false;
}

bool HatsDialog::HandleContextMenu(const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

}  // namespace chromeos
