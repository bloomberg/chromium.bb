// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/oauth2_mint_token_flow.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using content::URLFetcher;
using net::URLRequestContextGetter;
using net::URLRequestStatus;

namespace {

static const char kForceValueFalse[] = "false";
static const char kForceValueTrue[] = "true";
static const char kResponseTypeValueNone[] = "none";
static const char kResponseTypeValueToken[] = "token";

static const char kOAuth2IssueTokenBodyFormat[] =
    "force=%s"
    "&response_type=%s"
    "&scope=%s"
    "&client_id=%s"
    "&origin=%s";
static const char kIssueAdviceKey[] = "issueAdvice";
static const char kIssueAdviceValueAuto[] = "auto";
static const char kIssueAdviceValueConsent[] = "consent";
static const char kAccessTokenKey[] = "token";
static const char kConsentKey[] = "consent";
static const char kScopesKey[] = "scopes";
static const char kDescriptionKey[] = "description";
static const char kDetailKey[] = "detail";
static const char kDetailSeparators[] = "\n";

static GoogleServiceAuthError CreateAuthError(URLRequestStatus status) {
  CHECK(!status.is_success());
  if (status.status() == URLRequestStatus::CANCELED) {
    return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
  } else {
    DLOG(WARNING) << "Could not reach Google Accounts servers: errno "
                  << status.error();
    return GoogleServiceAuthError::FromConnectionError(status.error());
  }
}

OAuth2MintTokenFlow::InterceptorForTests* g_interceptor_for_tests = NULL;

}  // namespace

IssueAdviceInfoEntry::IssueAdviceInfoEntry() {}
IssueAdviceInfoEntry::~IssueAdviceInfoEntry() {}

bool IssueAdviceInfoEntry::operator ==(const IssueAdviceInfoEntry& rhs) const {
  return description == rhs.description && details == rhs.details;
}

OAuth2MintTokenFlow::Parameters::Parameters() : mode(MODE_ISSUE_ADVICE) {}

OAuth2MintTokenFlow::Parameters::Parameters(
    const std::string& rt,
    const std::string& eid,
    const std::string& cid,
    const std::vector<std::string>& scopes_arg,
    Mode mode_arg)
    : login_refresh_token(rt),
      extension_id(eid),
      client_id(cid),
      scopes(scopes_arg),
      mode(mode_arg) {
}

OAuth2MintTokenFlow::Parameters::~Parameters() {}

// static
void OAuth2MintTokenFlow::SetInterceptorForTests(
    OAuth2MintTokenFlow::InterceptorForTests* interceptor) {
  CHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType));
  CHECK(NULL == g_interceptor_for_tests);  // Only one at a time.
  g_interceptor_for_tests = interceptor;
}

OAuth2MintTokenFlow::OAuth2MintTokenFlow(
    URLRequestContextGetter* context,
    Delegate* delegate,
    const Parameters& parameters)
    : OAuth2ApiCallFlow(
          context, parameters.login_refresh_token,
          "", std::vector<std::string>()),
      context_(context),
      delegate_(delegate),
      parameters_(parameters) {
}

OAuth2MintTokenFlow::~OAuth2MintTokenFlow() { }

void OAuth2MintTokenFlow::Start() {
  if (g_interceptor_for_tests) {
    std::string auth_token;
    GoogleServiceAuthError error = GoogleServiceAuthError::None();

    // We use PostTask, instead of calling the delegate directly, because the
    // message loop will run a few times before we notify the delegate in the
    // real implementation.
    if (g_interceptor_for_tests->DoIntercept(this, &auth_token, &error)) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&OAuth2MintTokenFlow::Delegate::OnMintTokenSuccess,
                     base::Unretained(delegate_), auth_token));
    } else {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&OAuth2MintTokenFlow::Delegate::OnMintTokenFailure,
                     base::Unretained(delegate_), error));
    }
    return;
  }

  OAuth2ApiCallFlow::Start();
}

void OAuth2MintTokenFlow::ReportSuccess(const std::string& access_token) {
  if (delegate_) {
    delegate_->OnMintTokenSuccess(access_token);
  }
}

void OAuth2MintTokenFlow::ReportSuccess(const IssueAdviceInfo& issue_advice) {
  if (delegate_) {
    delegate_->OnIssueAdviceSuccess(issue_advice);
  }
}

void OAuth2MintTokenFlow::ReportFailure(
    const GoogleServiceAuthError& error) {
  if (delegate_) {
    delegate_->OnMintTokenFailure(error);
  }
}

GURL OAuth2MintTokenFlow::CreateApiCallUrl() {
  return GURL(GaiaUrls::GetInstance()->oauth2_issue_token_url());
}

std::string OAuth2MintTokenFlow::CreateApiCallBody() {
  const char* force_value =
      (parameters_.mode == MODE_MINT_TOKEN_FORCE ||
       parameters_.mode == MODE_RECORD_GRANT)
          ? kForceValueTrue : kForceValueFalse;
  const char* response_type_value =
      (parameters_.mode == MODE_MINT_TOKEN_NO_FORCE ||
       parameters_.mode == MODE_MINT_TOKEN_FORCE)
          ? kResponseTypeValueToken : kResponseTypeValueNone;
  return StringPrintf(
      kOAuth2IssueTokenBodyFormat,
      net::EscapeUrlEncodedData(force_value, true).c_str(),
      net::EscapeUrlEncodedData(response_type_value, true).c_str(),
      net::EscapeUrlEncodedData(
          JoinString(parameters_.scopes, ' '), true).c_str(),
      net::EscapeUrlEncodedData(parameters_.client_id, true).c_str(),
      net::EscapeUrlEncodedData(parameters_.extension_id, true).c_str());
}

void OAuth2MintTokenFlow::ProcessApiCallSuccess(
    const net::URLFetcher* source) {
  // TODO(munjal): Change error code paths in this method to report an
  // internal error.
  std::string response_body;
  source->GetResponseAsString(&response_body);
  scoped_ptr<base::Value> value(base::JSONReader::Read(response_body));
  DictionaryValue* dict = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict)) {
    ReportFailure(GoogleServiceAuthError::FromConnectionError(101));
    return;
  }

  std::string issue_advice;
  if (!dict->GetString(kIssueAdviceKey, &issue_advice)) {
    ReportFailure(GoogleServiceAuthError::FromConnectionError(101));
    return;
  }
  if (issue_advice == kIssueAdviceValueConsent) {
    IssueAdviceInfo issue_advice;
    if (ParseIssueAdviceResponse(dict, &issue_advice))
      ReportSuccess(issue_advice);
    else
      ReportFailure(GoogleServiceAuthError::FromConnectionError(101));
  } else {
    std::string access_token;
    if (ParseMintTokenResponse(dict, &access_token))
      ReportSuccess(access_token);
    else
      ReportFailure(GoogleServiceAuthError::FromConnectionError(101));
  }
}

void OAuth2MintTokenFlow::ProcessApiCallFailure(
    const net::URLFetcher* source) {
  ReportFailure(CreateAuthError(source->GetStatus()));
}
void OAuth2MintTokenFlow::ProcessNewAccessToken(
    const std::string& access_token) {
  // We don't currently store new access tokens. We generate one every time.
  // So we have nothing to do here.
  return;
}
void OAuth2MintTokenFlow::ProcessMintAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  ReportFailure(error);
}

// static
bool OAuth2MintTokenFlow::ParseMintTokenResponse(
    const base::DictionaryValue* dict, std::string* access_token) {
  CHECK(dict);
  CHECK(access_token);
  return dict->GetString(kAccessTokenKey, access_token);
}

// static
bool OAuth2MintTokenFlow::ParseIssueAdviceResponse(
    const base::DictionaryValue* dict, IssueAdviceInfo* issue_advice) {
  CHECK(dict);
  CHECK(issue_advice);

  base::DictionaryValue* consent_dict = NULL;
  if (!dict->GetDictionary(kConsentKey, &consent_dict))
    return false;

  base::ListValue* scopes_list = NULL;
  if (!consent_dict->GetList(kScopesKey, &scopes_list))
    return false;

  bool success = true;
  for (size_t index = 0; index < scopes_list->GetSize(); ++index) {
    base::DictionaryValue* scopes_entry = NULL;
    IssueAdviceInfoEntry entry;
    std::string detail;
    if (!scopes_list->GetDictionary(index, &scopes_entry) ||
        !scopes_entry->GetString(kDescriptionKey, &entry.description) ||
        !scopes_entry->GetString(kDetailKey, &detail)) {
      success = false;
      break;
    }

    Tokenize(detail, kDetailSeparators, &entry.details);
    issue_advice->push_back(entry);
  }

  if (!success)
    issue_advice->clear();

  return success;
}
