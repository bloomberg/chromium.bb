// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/setup_flow_login_step.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/remoting/setup_flow_get_status_step.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"

namespace remoting {

static const wchar_t kLoginIFrameXPath[] = L"//iframe[@id='login']";

SetupFlowLoginStep::SetupFlowLoginStep() { }

SetupFlowLoginStep::SetupFlowLoginStep(const string16& error_message)
    : error_message_(error_message) {
}

SetupFlowLoginStep::~SetupFlowLoginStep() { }

void SetupFlowLoginStep::HandleMessage(const std::string& message,
                                       const Value* arg) {
  if (message == "SubmitAuth") {
    DCHECK(arg);

    std::string json;
    if (!arg->GetAsString(&json) || json.empty()) {
      NOTREACHED();
      return;
    }

    scoped_ptr<Value> parsed_value(base::JSONReader::Read(json, false));
    if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY)) {
      NOTREACHED() << "Unable to parse auth data";
      return;
    }

    CHECK(parsed_value->IsType(Value::TYPE_DICTIONARY));

    std::string username, password, captcha, access_code;
    const DictionaryValue* result =
        static_cast<const DictionaryValue*>(parsed_value.get());
    if (!result->GetString("user", &username) ||
        !result->GetString("pass", &password) ||
        !result->GetString("captcha", &captcha) ||
        !result->GetString("access_code", &access_code)) {
      NOTREACHED() << "Unable to parse auth data";
      return;
    }

    OnUserSubmittedAuth(username, password, captcha, access_code);
  }
}

void SetupFlowLoginStep::Cancel() {
  if (authenticator_.get())
    authenticator_->CancelRequest();
}

void SetupFlowLoginStep::OnUserSubmittedAuth(const std::string& user,
                                             const std::string& password,
                                             const std::string& captcha,
                                             const std::string& access_code) {
  flow()->context()->login = user;

  // Start the authenticator.
  authenticator_.reset(
      new GaiaAuthFetcher(this, GaiaConstants::kChromeSource,
                          flow()->profile()->GetRequestContext()));

  std::string remoting_password;
  if (!access_code.empty())
    remoting_password = access_code;
  else
    remoting_password = password;

  authenticator_->StartClientLogin(user, remoting_password,
                                   GaiaConstants::kRemotingService,
                                   "", captcha,
                                   GaiaAuthFetcher::HostedAccountsAllowed);
}

void SetupFlowLoginStep::OnClientLoginSuccess(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // Save the token for remoting.
  flow()->context()->remoting_token = credentials.token;

  // After login has succeeded try to fetch the token for sync.
  // We need the token for sync to connect to the talk network.
  authenticator_->StartIssueAuthToken(credentials.sid, credentials.lsid,
                                      GaiaConstants::kSyncService);
}

void SetupFlowLoginStep::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  ShowGaiaFailed(error);
  authenticator_.reset();
}

void SetupFlowLoginStep::OnIssueAuthTokenSuccess(
    const std::string& service, const std::string& auth_token) {
  // Save the sync token.
  flow()->context()->talk_token = auth_token;
  authenticator_.reset();

  FinishStep(new SetupFlowGetStatusStep());
}

void SetupFlowLoginStep::OnIssueAuthTokenFailure(const std::string& service,
    const GoogleServiceAuthError& error) {
  ShowGaiaFailed(error);
  authenticator_.reset();
}

void SetupFlowLoginStep::DoStart() {
  DictionaryValue args;
  // TODO(sergeyu): Supply current login name if the service was started before.
  args.SetString("user", "");
  args.SetBoolean("editable_user", true);
  if (!error_message_.empty())
    args.SetString("error_message", error_message_);
  ShowGaiaLogin(args);
}

void SetupFlowLoginStep::ShowGaiaLogin(const DictionaryValue& args) {
  WebUI* web_ui = flow()->web_ui();
  DCHECK(web_ui);

  web_ui->CallJavascriptFunction("showLogin");

  std::string json;
  base::JSONWriter::Write(&args, false, &json);
  std::wstring javascript = std::wstring(L"showGaiaLogin(") +
      UTF8ToWide(json) + L");";
  ExecuteJavascriptInIFrame(kLoginIFrameXPath, javascript);
}

void SetupFlowLoginStep::ShowGaiaFailed(const GoogleServiceAuthError& error) {
  DictionaryValue args;
  args.SetInteger("error", error.state());
  args.SetBoolean("editable_user", true);
  args.SetString("captchaUrl", error.captcha().image_url.spec());
  ShowGaiaLogin(args);
}

}  // namespace remoting
