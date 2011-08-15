// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/gaia_urls.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

namespace {

const char kDefaultGaiaBaseUrl[] = "www.google.com";
const char kCaptchaUrlPrefixSuffix[] = "/accounts/";
const char kClientLoginUrlSuffix[] = "/accounts/ClientLogin";
const char kIssueAuthTokenUrlSuffix[] = "/accounts/IssueAuthToken";
const char kGetUserInfoUrlSuffix[] = "/accounts/GetUserInfo";
const char kTokenAuthUrlSuffix[] = "/accounts/TokenAuth";
const char kMergeSessionUrlSuffix[] = "/accounts/MergeSession";

}  // namespacce

GaiaUrls* GaiaUrls::GetInstance() {
  return Singleton<GaiaUrls>::get();
}

GaiaUrls::GaiaUrls() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGaiaHost)) {
    host_base_ = command_line->GetSwitchValueASCII(switches::kGaiaHost);
  } else {
    host_base_ = kDefaultGaiaBaseUrl;
  }

  captcha_url_prefix_ = "http://" + host_base_ + kCaptchaUrlPrefixSuffix;
  client_login_url_ = "https://" + host_base_ + kClientLoginUrlSuffix;
  issue_auth_token_url_ = "https://" + host_base_ + kIssueAuthTokenUrlSuffix;
  get_user_info_url_ = "https://" + host_base_ + kGetUserInfoUrlSuffix;
  token_auth_url_ = "https://" + host_base_ + kTokenAuthUrlSuffix;
  merge_session_url_ = "https://" + host_base_ + kMergeSessionUrlSuffix;
}

GaiaUrls::~GaiaUrls() {
}

const std::string& GaiaUrls::captcha_url_prefix() {
  return captcha_url_prefix_;
}

const std::string& GaiaUrls::client_login_url() {
  return client_login_url_;
}

const std::string& GaiaUrls::issue_auth_token_url() {
  return issue_auth_token_url_;
}

const std::string& GaiaUrls::get_user_info_url() {
  return get_user_info_url_;
}

const std::string& GaiaUrls::token_auth_url() {
  return token_auth_url_;
}

const std::string& GaiaUrls::merge_session_url() {
  return merge_session_url_;
}
