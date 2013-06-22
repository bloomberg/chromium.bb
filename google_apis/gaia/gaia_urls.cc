// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_urls.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/google_api_keys.h"

namespace {

// Gaia service constants
const char kDefaultGaiaUrl[] = "https://accounts.google.com";
const char kDefaultGoogleApisBaseUrl[] = "www.googleapis.com";
const char kCaptchaUrlPrefixSuffix[] = "/";

// API calls from accounts.google.com
const char kClientLoginUrlSuffix[] = "/ClientLogin";
const char kServiceLoginUrlSuffix[] = "/ServiceLogin";
const char kServiceLogoutUrlSuffix[] = "/Logout";
const char kIssueAuthTokenUrlSuffix[] = "/IssueAuthToken";
const char kGetUserInfoUrlSuffix[] = "/GetUserInfo";
const char kTokenAuthUrlSuffix[] = "/TokenAuth";
const char kMergeSessionUrlSuffix[] = "/MergeSession";
const char kOAuthGetAccessTokenUrlSuffix[] = "/OAuthGetAccessToken";
const char kOAuthWrapBridgeUrlSuffix[] = "/OAuthWrapBridge";
const char kOAuth1LoginUrlSuffix[] = "/OAuthLogin";
const char kOAuthRevokeTokenUrlSuffix[] = "/AuthSubRevokeToken";

// API calls from accounts.google.com (LSO)
const char kGetOAuthTokenUrlSuffix[] = "/o/oauth/GetOAuthToken/";
const char kClientLoginToOAuth2UrlSuffix[] = "/o/oauth2/programmatic_auth";
const char kOAuth2AuthUrlSuffix[] = "/o/oauth2/auth";
const char kOAuth2RevokeUrlSuffix[] = "/o/oauth2/revoke";
const char kOAuth2TokenUrlSuffix[] = "/o/oauth2/token";
const char kClientOAuthUrlSuffix[] = "/ClientOAuth";

// API calls from www.googleapis.com
const char kOAuth2IssueTokenUrlSuffix[] = "/oauth2/v2/IssueToken";
const char kOAuth2TokenInfoUrlSuffix[] = "/oauth2/v2/tokeninfo";
const char kOAuthUserInfoUrlSuffix[] = "/oauth2/v1/userinfo";
const char kOAuthWrapBridgeUserInfoScopeUrlSuffix[] = "/auth/userinfo.email";

const char kOAuth1LoginScope[] =
    "https://www.google.com/accounts/OAuthLogin";

void GetSwitchValueWithDefault(const char* switch_value,
                               const char* default_value,
                               std::string* output_value) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_value)) {
    *output_value = command_line->GetSwitchValueASCII(switch_value);
  } else {
    *output_value = default_value;
  }
}

}  // namespace

GaiaUrls* GaiaUrls::GetInstance() {
  return Singleton<GaiaUrls>::get();
}

GaiaUrls::GaiaUrls() {
  std::string gaia_url_str;
  GetSwitchValueWithDefault(switches::kGaiaUrl,
                            kDefaultGaiaUrl,
                            &gaia_url_str);
  gaia_url_ = GURL(gaia_url_str);
  DCHECK(gaia_url_.is_valid());

  GetSwitchValueWithDefault(switches::kLsoUrl,
                            kDefaultGaiaUrl,
                            &lso_origin_url_);

  std::string google_apis_base;
  GetSwitchValueWithDefault(switches::kGoogleApisHost,
                            kDefaultGoogleApisBaseUrl,
                            &google_apis_base);

  captcha_url_prefix_ = "http://" + gaia_url_.host() +
                        (gaia_url_.has_port() ? ":" + gaia_url_.port() : "") +
                        kCaptchaUrlPrefixSuffix;

  google_apis_origin_url_ = "https://" + google_apis_base;

  oauth2_chrome_client_id_ =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  oauth2_chrome_client_secret_ =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);

  // URLs from accounts.google.com.
  gaia_login_form_realm_ = gaia_url_str + "/";
  client_login_url_ = gaia_url_str + kClientLoginUrlSuffix;
  service_login_url_ = gaia_url_str + kServiceLoginUrlSuffix;
  service_logout_url_ = gaia_url_str + kServiceLogoutUrlSuffix;
  issue_auth_token_url_ = gaia_url_str + kIssueAuthTokenUrlSuffix;
  get_user_info_url_ = gaia_url_str + kGetUserInfoUrlSuffix;
  token_auth_url_ = gaia_url_str + kTokenAuthUrlSuffix;
  merge_session_url_ = gaia_url_str + kMergeSessionUrlSuffix;
  oauth_get_access_token_url_ = gaia_url_str + kOAuthGetAccessTokenUrlSuffix;
  oauth_wrap_bridge_url_ = gaia_url_str + kOAuthWrapBridgeUrlSuffix;
  oauth_revoke_token_url_ = gaia_url_str + kOAuthRevokeTokenUrlSuffix;
  oauth1_login_url_ = gaia_url_str + kOAuth1LoginUrlSuffix;

  // URLs from accounts.google.com (LSO).
  get_oauth_token_url_ = lso_origin_url_ + kGetOAuthTokenUrlSuffix;
  std::string client_login_to_oauth2_url = lso_origin_url_ +
                                           kClientLoginToOAuth2UrlSuffix;
  oauth2_auth_url_ = lso_origin_url_ + kOAuth2AuthUrlSuffix;
  std::string oauth2_token_url = lso_origin_url_ + kOAuth2TokenUrlSuffix;
  oauth2_revoke_url_ = lso_origin_url_ + kOAuth2RevokeUrlSuffix;

  // URLs from www.googleapis.com.
  oauth_wrap_bridge_user_info_scope_ = google_apis_origin_url_ +
      kOAuthWrapBridgeUserInfoScopeUrlSuffix;
  std::string oauth2_issue_token_url = google_apis_origin_url_ +
                                       kOAuth2IssueTokenUrlSuffix;
  oauth2_token_info_url_ = google_apis_origin_url_ + kOAuth2TokenInfoUrlSuffix;
  std::string oauth_user_info_url = google_apis_origin_url_ +
                                    kOAuthUserInfoUrlSuffix;

  // TODO(zelidrag): Get rid of all these switches since all URLs should be
  // controlled only with --gaia-url, --lso-url and --google-apis-host.
  GetSwitchValueWithDefault(switches::kOAuth1LoginScope,
                            kOAuth1LoginScope,
                            &oauth1_login_scope_);
  GetSwitchValueWithDefault(switches::kClientLoginToOAuth2Url,
                            client_login_to_oauth2_url.c_str(),
                            &client_login_to_oauth2_url_);
  GetSwitchValueWithDefault(switches::kOAuth2TokenUrl,
                            oauth2_token_url.c_str(),
                            &oauth2_token_url_);
  GetSwitchValueWithDefault(switches::kOAuth2IssueTokenUrl,
                            oauth2_issue_token_url.c_str(),
                            &oauth2_issue_token_url_);
  GetSwitchValueWithDefault(switches::kOAuthUserInfoUrl,
                            oauth_user_info_url.c_str(),
                            &oauth_user_info_url_);
}

GaiaUrls::~GaiaUrls() {
}

const std::string& GaiaUrls::captcha_url_prefix() const {
  return captcha_url_prefix_;
}

const GURL& GaiaUrls::gaia_url() const {
  return gaia_url_;
}

const std::string& GaiaUrls::client_login_url() const {
  return client_login_url_;
}

const std::string& GaiaUrls::service_login_url() const {
  return service_login_url_;
}

const std::string& GaiaUrls::service_logout_url() const {
  return service_logout_url_;
}

const std::string& GaiaUrls::issue_auth_token_url() const {
  return issue_auth_token_url_;
}

const std::string& GaiaUrls::get_user_info_url() const {
  return get_user_info_url_;
}

const std::string& GaiaUrls::token_auth_url() const {
  return token_auth_url_;
}

const std::string& GaiaUrls::merge_session_url() const {
  return merge_session_url_;
}

const std::string& GaiaUrls::get_oauth_token_url() const {
  return get_oauth_token_url_;
}

const std::string& GaiaUrls::oauth_get_access_token_url() const {
  return oauth_get_access_token_url_;
}

const std::string& GaiaUrls::oauth_wrap_bridge_url() const {
  return oauth_wrap_bridge_url_;
}

const std::string& GaiaUrls::oauth_user_info_url() const {
  return oauth_user_info_url_;
}

const std::string& GaiaUrls::oauth_revoke_token_url() const {
  return oauth_revoke_token_url_;
}

const std::string& GaiaUrls::oauth1_login_url() const {
  return oauth1_login_url_;
}

const std::string& GaiaUrls::oauth1_login_scope() const {
  return oauth1_login_scope_;
}

const std::string& GaiaUrls::oauth_wrap_bridge_user_info_scope() const {
  return oauth_wrap_bridge_user_info_scope_;
}

const std::string& GaiaUrls::oauth2_chrome_client_id() const {
  return oauth2_chrome_client_id_;
}

const std::string& GaiaUrls::oauth2_chrome_client_secret() const {
  return oauth2_chrome_client_secret_;
}

const std::string& GaiaUrls::client_login_to_oauth2_url() const {
  return client_login_to_oauth2_url_;
}

const std::string& GaiaUrls::oauth2_auth_url() const {
  return oauth2_auth_url_;
}

const std::string& GaiaUrls::oauth2_token_url() const {
  return oauth2_token_url_;
}

const std::string& GaiaUrls::oauth2_issue_token_url() const {
  return oauth2_issue_token_url_;
}

const std::string& GaiaUrls::oauth2_token_info_url() const {
  return oauth2_token_info_url_;
}

const std::string& GaiaUrls::oauth2_revoke_url() const {
  return oauth2_revoke_url_;
}

const std::string& GaiaUrls::gaia_login_form_realm() const {
  return gaia_login_form_realm_;
}
