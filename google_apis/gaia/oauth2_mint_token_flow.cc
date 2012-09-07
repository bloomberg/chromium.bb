// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_mint_token_flow.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using net::URLFetcher;
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
  if (status.status() == URLRequestStatus::CANCELED) {
    return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
  } else {
    // TODO(munjal): Improve error handling. Currently we return connection
    // error for even application level errors. We need to either expand the
    // GoogleServiceAuthError enum or create a new one to report better
    // errors.
    DLOG(WARNING) << "Server returned error: errno " << status.error();
    return GoogleServiceAuthError::FromConnectionError(status.error());
  }
}

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

OAuth2MintTokenFlow::OAuth2MintTokenFlow(
    URLRequestContextGetter* context,
    Delegate* delegate,
    const Parameters& parameters)
    : OAuth2ApiCallFlow(
          context, parameters.login_refresh_token,
          "", std::vector<std::string>()),
      context_(context),
      delegate_(delegate),
      parameters_(parameters),
      delete_when_done_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

OAuth2MintTokenFlow::~OAuth2MintTokenFlow() { }

void OAuth2MintTokenFlow::FireAndForget() {
  delete_when_done_ = true;
  Start();
}

void OAuth2MintTokenFlow::ReportSuccess(const std::string& access_token) {
  scoped_ptr<OAuth2MintTokenFlow> will_delete(delete_when_done_ ? this : NULL);

  if (delegate_)
    delegate_->OnMintTokenSuccess(access_token);

  // |this| may already be deleted.
}

void OAuth2MintTokenFlow::ReportIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  scoped_ptr<OAuth2MintTokenFlow> will_delete(delete_when_done_ ? this : NULL);

  if (delegate_)
    delegate_->OnIssueAdviceSuccess(issue_advice);

  // |this| may already be deleted.
}

void OAuth2MintTokenFlow::ReportFailure(
    const GoogleServiceAuthError& error) {
  scoped_ptr<OAuth2MintTokenFlow> will_delete(delete_when_done_ ? this : NULL);

  if (delegate_)
    delegate_->OnMintTokenFailure(error);

  // |this| may already be deleted.
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
      ReportIssueAdviceSuccess(issue_advice);
    else
      ReportFailure(GoogleServiceAuthError::FromConnectionError(101));
  } else {
    std::string access_token;
    if (ParseMintTokenResponse(dict, &access_token))
      ReportSuccess(access_token);
    else
      ReportFailure(GoogleServiceAuthError::FromConnectionError(101));
  }

  // |this| may be deleted!
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

  const base::DictionaryValue* consent_dict = NULL;
  if (!dict->GetDictionary(kConsentKey, &consent_dict))
    return false;

  const base::ListValue* scopes_list = NULL;
  if (!consent_dict->GetList(kScopesKey, &scopes_list))
    return false;

  bool success = true;
  for (size_t index = 0; index < scopes_list->GetSize(); ++index) {
    const base::DictionaryValue* scopes_entry = NULL;
    IssueAdviceInfoEntry entry;
    string16 detail;
    if (!scopes_list->GetDictionary(index, &scopes_entry) ||
        !scopes_entry->GetString(kDescriptionKey, &entry.description) ||
        !scopes_entry->GetString(kDetailKey, &detail)) {
      success = false;
      break;
    }

    TrimWhitespace(entry.description, TRIM_ALL, &entry.description);
    static const string16 detail_separators = ASCIIToUTF16(kDetailSeparators);
    Tokenize(detail, detail_separators, &entry.details);
    for (size_t i = 0; i < entry.details.size(); i++)
      TrimWhitespace(entry.details[i], TRIM_ALL, &entry.details[i]);
    issue_advice->push_back(entry);
  }

  if (!success)
    issue_advice->clear();

  return success;
}
