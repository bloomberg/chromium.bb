// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/remoting_resources_source.h"

#include <algorithm>
#include <string>

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/common/jstemplate_builder.h"
#include "googleurl/src/gurl.h"
#include "grit/app_resources.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// Define the values of standard URLs.
const char RemotingResourcesSource::kInvalidPasswordHelpUrl[] =
  "http://www.google.com/support/accounts/bin/answer.py?ctx=ch&answer=27444";
const char RemotingResourcesSource::kCanNotAccessAccountUrl[] =
  "http://www.google.com/support/accounts/bin/answer.py?answer=48598";
const char RemotingResourcesSource::kCreateNewAccountUrl[] =
  "https://www.google.com/accounts/NewAccount?service=chromiumsync";

RemotingResourcesSource::RemotingResourcesSource()
  : DataSource(chrome::kChromeUIRemotingResourcesHost,
               MessageLoop::current()) {
}

void RemotingResourcesSource::StartDataRequest(const std::string& path_raw,
    bool is_off_the_record, int request_id) {
  const char kRemotingGaiaLoginPath[] = "gaialogin";
  const char kRemotingSetupFlowPath[] = "setup";
  const char kRemotingSetupDonePath[] = "setupdone";
  const char kRemotingSettingUpPath[] = "settingup";
  const char kRemotingSetupErrorPath[] = "setuperror";

  std::string response;
  if (path_raw == kRemotingGaiaLoginPath) {
    DictionaryValue localized_strings;

    // Start by setting the per-locale URLs we show on the setup wizard.
    localized_strings.SetString("invalidpasswordhelpurl",
        GetLocalizedUrl(kInvalidPasswordHelpUrl));
    localized_strings.SetString("cannotaccessaccounturl",
        GetLocalizedUrl(kCanNotAccessAccountUrl));
    localized_strings.SetString("createnewaccounturl",
        GetLocalizedUrl(kCreateNewAccountUrl));

    localized_strings.SetString("settingupsync",
        l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_SETTING_UP_SYNC));
    localized_strings.SetString("introduction", "");
    localized_strings.SetString("signinprefix",
        l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_SIGNIN_PREFIX));
    localized_strings.SetString("signinsuffix",
        l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_SIGNIN_SUFFIX));
    localized_strings.SetString("cannotbeblank",
        l10n_util::GetStringUTF16(IDS_SYNC_CANNOT_BE_BLANK));
    localized_strings.SetString("emaillabel",
        l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_EMAIL));
    localized_strings.SetString("passwordlabel",
        l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_PASSWORD));
    localized_strings.SetString("invalidcredentials",
        l10n_util::GetStringUTF16(IDS_SYNC_INVALID_USER_CREDENTIALS));
    localized_strings.SetString("signin",
        l10n_util::GetStringUTF16(IDS_SYNC_SIGNIN));
    localized_strings.SetString("couldnotconnect",
        l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_COULD_NOT_CONNECT));
    localized_strings.SetString("cannotaccessaccount",
        l10n_util::GetStringUTF16(IDS_SYNC_CANNOT_ACCESS_ACCOUNT));
    localized_strings.SetString("createaccount",
        l10n_util::GetStringUTF16(IDS_SYNC_CREATE_ACCOUNT));
    localized_strings.SetString("cancel",
        l10n_util::GetStringUTF16(IDS_CANCEL));
    localized_strings.SetString("settingup",
        l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_SETTING_UP));
    localized_strings.SetString("success",
        l10n_util::GetStringUTF16(IDS_SYNC_SUCCESS));
    localized_strings.SetString("errorsigningin",
        l10n_util::GetStringUTF16(IDS_SYNC_ERROR_SIGNING_IN));
    localized_strings.SetString("captchainstructions",
        l10n_util::GetStringUTF16(IDS_SYNC_GAIA_CAPTCHA_INSTRUCTIONS));
    localized_strings.SetString("invalidaccesscode",
        l10n_util::GetStringUTF16(IDS_SYNC_INVALID_ACCESS_CODE_LABEL));
    localized_strings.SetString("enteraccesscode",
        l10n_util::GetStringUTF16(IDS_SYNC_ENTER_ACCESS_CODE_LABEL));
    localized_strings.SetString("getaccesscodehelp",
        l10n_util::GetStringUTF16(IDS_SYNC_ACCESS_CODE_HELP_LABEL));
    localized_strings.SetString("getaccesscodeurl",
        l10n_util::GetStringUTF16(IDS_SYNC_GET_ACCESS_CODE_URL));

    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_GAIA_LOGIN_HTML));
    SetFontAndTextDirection(&localized_strings);
    response = jstemplate_builder::GetI18nTemplateHtml(
        html, &localized_strings);
  } else if (path_raw == kRemotingSettingUpPath) {
    DictionaryValue localized_strings;
    localized_strings.SetString("settingup",
        l10n_util::GetStringUTF16(IDS_REMOTING_SETTING_UP_MESSAGE));
    localized_strings.SetString("cancel",
        l10n_util::GetStringUTF16(IDS_CANCEL));
    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_REMOTING_SETTING_UP_HTML));
    SetFontAndTextDirection(&localized_strings);
    response = jstemplate_builder::GetI18nTemplateHtml(
        html, &localized_strings);
  } else if (path_raw == kRemotingSetupDonePath) {
    DictionaryValue localized_strings;
    localized_strings.SetString("success",
        l10n_util::GetStringUTF16(IDS_REMOTING_SUCCESS_TITLE));
    localized_strings.SetString("okay",
        l10n_util::GetStringUTF16(IDS_OK));
    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_REMOTING_SETUP_DONE_HTML));
    SetFontAndTextDirection(&localized_strings);
    response = jstemplate_builder::GetI18nTemplateHtml(
        html, &localized_strings);
  } else if (path_raw == kRemotingSetupErrorPath) {
    DictionaryValue localized_strings;
    localized_strings.SetString("close",
        l10n_util::GetStringUTF16(IDS_CLOSE));
    localized_strings.SetString("retry",
        l10n_util::GetStringUTF16(IDS_REMOTING_RETRY_BUTTON_TEXT));
    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_REMOTING_SETUP_ERROR_HTML));
    SetFontAndTextDirection(&localized_strings);
    response = jstemplate_builder::GetI18nTemplateHtml(
        html, &localized_strings);
  } else if (path_raw == kRemotingSetupFlowPath) {
    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_REMOTING_SETUP_FLOW_HTML));
    response = html.as_string();
  }
  // Send the response.
  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(response.size());
  std::copy(response.begin(), response.end(), html_bytes->data.begin());
  SendResponse(request_id, html_bytes);
}

std::string RemotingResourcesSource::GetMimeType(
    const std::string& path) const {
  return "text/html";
}

std::string RemotingResourcesSource::GetLocalizedUrl(
    const std::string& url) const {
  GURL original_url(url);
  DCHECK(original_url.is_valid());
  GURL localized_url = google_util::AppendGoogleLocaleParam(original_url);
  return localized_url.spec();
}
