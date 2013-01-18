// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_setup_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/jstemplate_builder.h"

// Define the values of standard URLs.
const char CloudPrintSetupSource::kInvalidPasswordHelpUrl[] =
  "https://www.google.com/support/accounts/bin/answer.py?ctx=ch&answer=27444";
const char CloudPrintSetupSource::kCanNotAccessAccountUrl[] =
  "https://www.google.com/support/accounts/bin/answer.py?answer=48598";
const char CloudPrintSetupSource::kCreateNewAccountUrl[] =
  "https://accounts.google.com/NewAccount?service=chromiumsync";

namespace {

// Utility method to keep dictionary population code streamlined.
void AddString(DictionaryValue* dictionary,
               const std::string& key,
               int resource_id) {
  dictionary->SetString(key, l10n_util::GetStringUTF16(resource_id));
}

}  // namespace

CloudPrintSetupSource::CloudPrintSetupSource() {
}

std::string CloudPrintSetupSource::GetSource() {
  return chrome::kChromeUICloudPrintSetupHost;
}

void CloudPrintSetupSource::StartDataRequest(
    const std::string& path_raw,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  const char kCloudPrintSetupPath[] = "cloudprintsetup";
  const char kCloudPrintGaiaLoginPath[] = "gaialogin";
  const char kCloudPrintSetupFlowPath[] = "setupflow";
  const char kCloudPrintSetupDonePath[] = "setupdone";

  DictionaryValue localized_strings;
  DictionaryValue* dict = &localized_strings;

  std::string response;
  if (path_raw == kCloudPrintSetupPath) {
    dict->SetString("header",
        l10n_util::GetStringFUTF16(IDS_CLOUD_PRINT_SETUP_HEADER,
        l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
    AddString(dict, "explain", IDS_CLOUD_PRINT_SETUP_EXPLAIN);
    AddString(dict, "anywhereheader", IDS_CLOUD_PRINT_SETUP_ANYWHERE_HEADER);
    AddString(dict, "anywhereexplain", IDS_CLOUD_PRINT_SETUP_ANYWHERE_EXPLAIN);
    AddString(dict, "printerheader", IDS_CLOUD_PRINT_SETUP_PRINTER_HEADER);
    AddString(dict, "printerexplain", IDS_CLOUD_PRINT_SETUP_PRINTER_EXPLAIN);
    AddString(dict, "sharingheader", IDS_CLOUD_PRINT_SETUP_SHARING_HEADER);
    AddString(dict, "sharingexplain", IDS_CLOUD_PRINT_SETUP_SHARING_EXPLAIN);

    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_CLOUD_PRINT_SETUP_LOGIN_HTML));
    web_ui_util::SetFontAndTextDirection(dict);
    response = jstemplate_builder::GetI18nTemplateHtml(html, dict);
  } else if (path_raw == kCloudPrintGaiaLoginPath) {
    // Start by setting the per-locale URLs we show on the setup wizard.
    dict->SetString("invalidpasswordhelpurl",
                   GetLocalizedUrl(kInvalidPasswordHelpUrl));
    dict->SetString("cannotaccessaccounturl",
                   GetLocalizedUrl(kCanNotAccessAccountUrl));
    dict->SetString("createnewaccounturl",
                   GetLocalizedUrl(kCreateNewAccountUrl));

    // None of the strings used here currently have sync-specific wording in
    // them.  There is a unit test to catch if that happens.
    dict->SetString("introduction", "");
    AddString(dict, "signinprefix", IDS_SYNC_LOGIN_SIGNIN_PREFIX);
    AddString(dict, "signinsuffix", IDS_SYNC_LOGIN_SIGNIN_SUFFIX);
    AddString(dict, "cannotbeblank", IDS_SYNC_CANNOT_BE_BLANK);
    AddString(dict, "emaillabel", IDS_SYNC_LOGIN_EMAIL_SAME_LINE);
    AddString(dict, "passwordlabel", IDS_SYNC_LOGIN_PASSWORD_SAME_LINE);
    AddString(dict, "invalidcredentials", IDS_SYNC_INVALID_USER_CREDENTIALS);
    AddString(dict, "signin", IDS_SYNC_SIGNIN);
    AddString(dict, "couldnotconnect", IDS_SYNC_LOGIN_COULD_NOT_CONNECT);
    AddString(dict, "cannotaccessaccount", IDS_SYNC_CANNOT_ACCESS_ACCOUNT);
    AddString(dict, "createaccount", IDS_SYNC_CREATE_ACCOUNT);
    AddString(dict, "cancel", IDS_CANCEL);
    AddString(dict, "settingup", IDS_SYNC_LOGIN_SETTING_UP);
    AddString(dict, "success", IDS_SYNC_SUCCESS);
    AddString(dict, "errorsigningin", IDS_SYNC_ERROR_SIGNING_IN);
    AddString(dict, "captchainstructions", IDS_SYNC_GAIA_CAPTCHA_INSTRUCTIONS);
    AddString(dict, "invalidaccesscode", IDS_SYNC_INVALID_ACCESS_CODE_LABEL);
    AddString(dict, "enteraccesscode", IDS_SYNC_ENTER_ACCESS_CODE_LABEL);
    AddString(dict, "getaccesscodehelp", IDS_SYNC_ACCESS_CODE_HELP_LABEL);
    AddString(dict, "getaccesscodeurl", IDS_SYNC_GET_ACCESS_CODE_URL);

    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_GAIA_LOGIN_HTML));
    web_ui_util::SetFontAndTextDirection(dict);
    response = jstemplate_builder::GetI18nTemplateHtml(html, dict);
  } else if (path_raw == kCloudPrintSetupDonePath) {
    AddString(dict, "testpage", IDS_CLOUD_PRINT_SETUP_TEST_PAGE);
    AddString(dict, "success", IDS_SYNC_SUCCESS);
    AddString(dict, "okay", IDS_SYNC_SETUP_OK_BUTTON_LABEL);
    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_CLOUD_PRINT_SETUP_DONE_HTML));
    web_ui_util::SetFontAndTextDirection(dict);
    response = jstemplate_builder::GetI18nTemplateHtml(html, dict);
  } else if (path_raw == kCloudPrintSetupFlowPath) {
    static const base::StringPiece html(
        ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_CLOUD_PRINT_SETUP_FLOW_HTML));
    response = html.as_string();
  }

  callback.Run(base::RefCountedString::TakeString(&response));
}

std::string CloudPrintSetupSource::GetMimeType(const std::string& path) const {
  return "text/html";
}

std::string CloudPrintSetupSource::GetLocalizedUrl(
    const std::string& url) const {
  GURL original_url(url);
  DCHECK(original_url.is_valid());
  GURL localized_url = google_util::AppendGoogleLocaleParam(original_url);
  return localized_url.spec();
}
