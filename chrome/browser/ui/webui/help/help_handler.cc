// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/help_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Returns the browser version as a string.
string16 BuildBrowserVersionString() {
  chrome::VersionInfo version_info;
  DCHECK(version_info.is_valid());

  std::string browser_version = version_info.Version();
  std::string version_modifier =
      chrome::VersionInfo::GetVersionStringModifier();
  if (!version_modifier.empty())
    browser_version += " " + version_modifier;

#if !defined(GOOGLE_CHROME_BUILD)
  browser_version += " (";
  browser_version += version_info.LastChange();
  browser_version += ")";
#endif

  return UTF8ToUTF16(browser_version);
}

}  // namespace

HelpHandler::HelpHandler()
    : version_updater_(VersionUpdater::Create()) {
}

HelpHandler::~HelpHandler() {
}

void HelpHandler::GetLocalizedValues(DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  DCHECK(localized_strings->empty());

  struct L10nResources {
    const char* name;
    int ids;
  };

  static L10nResources resources[] = {
    { "helpTitle", IDS_HELP_TITLE },
    { "aboutProductTitle", IDS_ABOUT_CHROME_TITLE },
    { "aboutProductDescription", IDS_ABOUT_PRODUCT_DESCRIPTION },
    { "relaunch", IDS_RELAUNCH_BUTTON },
    { "productName", IDS_PRODUCT_NAME },
    { "productCopyright", IDS_ABOUT_VERSION_COPYRIGHT },
    { "updateCheckStarted", IDS_UPGRADE_CHECK_STARTED },
    { "upToDate", IDS_UPGRADE_UP_TO_DATE },
    { "updating", IDS_UPGRADE_UPDATING },
    { "updateAlmostDone", IDS_UPGRADE_SUCCESSFUL_RELAUNCH },
    // TODO(jhawkins): Verify the following UI is only in the official build.
#if defined(OFFICIAL_BUILD)
    { "getHelpWithChrome",  IDS_GET_HELP_USING_CHROME },
    { "reportAProblem",  IDS_REPORT_A_PROBLEM },
#endif
#if defined(OS_CHROMEOS)
    { "platform", IDS_PLATFORM_LABEL },
    { "firmware", IDS_ABOUT_PAGE_FIRMWARE },
#endif
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(resources); ++i) {
    localized_strings->SetString(resources[i].name,
                                 l10n_util::GetStringUTF16(resources[i].ids));
  }

  localized_strings->SetString(
      "browserVersion",
      l10n_util::GetStringFUTF16(IDS_ABOUT_PRODUCT_VERSION,
                                 BuildBrowserVersionString()));

  string16 license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_VERSION_LICENSE,
#if !defined(OS_CHROMEOS)
      UTF8ToUTF16(google_util::StringAppendGoogleLocaleParam(
          chrome::kChromiumProjectURL)),
#endif
      ASCIIToUTF16(chrome::kChromeUICreditsURL));
  localized_strings->SetString("productLicense", license);

  string16 tos = l10n_util::GetStringFUTF16(
      IDS_ABOUT_TERMS_OF_SERVICE, UTF8ToUTF16(chrome::kChromeUITermsURL));
  localized_strings->SetString("productTOS", tos);
}

void HelpHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("onPageLoaded",
      base::Bind(&HelpHandler::OnPageLoaded, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("relaunchNow",
      base::Bind(&HelpHandler::RelaunchNow, base::Unretained(this)));
}

void HelpHandler::OnPageLoaded(const ListValue* args) {
  #if defined(OS_CHROMEOS)
  // Version information is loaded from a callback
  loader_.GetVersion(&consumer_, base::Bind(&HelpHandler::OnOSVersion,
                                            base::Unretained(this)),
                     chromeos::VersionLoader::VERSION_FULL);
  loader_.GetFirmware(&consumer_, base::Bind(&HelpHandler::OnOSFirmware,
                                             base::Unretained(this)));
#endif  // defined(OS_CHROMEOS)

  version_updater_->CheckForUpdate(
      base::Bind(&HelpHandler::UpdateStatus, base::Unretained(this)));
}

void HelpHandler::RelaunchNow(const ListValue* args) {
  version_updater_->RelaunchBrowser();
}

void HelpHandler::UpdateStatus(VersionUpdater::Status status, int progress) {
  // Only UPDATING state should have progress set.
  DCHECK(status == VersionUpdater::UPDATING || progress == 0);

  std::string status_str;
  switch (status) {
  case VersionUpdater::CHECKING:
    status_str = "checking";
    break;
  case VersionUpdater::UPDATING:
    status_str = "updating";
    break;
  case VersionUpdater::NEARLY_UPDATED:
    status_str = "nearly_updated";
    break;
  case VersionUpdater::UPDATED:
    status_str = "updated";
    break;
  }

  scoped_ptr<Value> status_value(Value::CreateStringValue(status_str));
  web_ui()->CallJavascriptFunction("HelpPage.setUpdateStatus", *status_value);

  if (status == VersionUpdater::UPDATING) {
    scoped_ptr<Value> progress_value(Value::CreateIntegerValue(progress));
    web_ui()->CallJavascriptFunction("HelpPage.setProgress", *progress_value);
  }
}

#if defined(OS_CHROMEOS)

void HelpHandler::OnOSVersion(chromeos::VersionLoader::Handle handle,
                              std::string version) {
  if (version.size()) {
    scoped_ptr<Value> version_string(Value::CreateStringValue(version));
    web_ui()->CallJavascriptFunction("HelpPage.setOSVersion", *version_string);
  }
}

void HelpHandler::OnOSFirmware(chromeos::VersionLoader::Handle handle,
                               std::string firmware) {
  if (firmware.size()) {
    scoped_ptr<Value> firmware_string(Value::CreateStringValue(firmware));
    web_ui()->CallJavascriptFunction("HelpPage.setOSFirmware",
                                     *firmware_string);
  }
}

#endif // defined(OS_CHROMEOS)
