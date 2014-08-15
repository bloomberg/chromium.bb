// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_TOGGLE_FLOW_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_TOGGLE_FLOW_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "google_apis/gaia/oauth2_token_service.h"

class Profile;

class EasyUnlockToggleFlow : public OAuth2TokenService::Consumer,
                             public OAuth2MintTokenFlow::Delegate {
 public:
  // Callback to indicate whether the call succeeds or not.
  typedef base::Callback<void(bool)> ToggleFlowCallback;

  EasyUnlockToggleFlow(Profile* profile,
                       const std::string& phone_public_key,
                       bool toggle_enable,
                       const ToggleFlowCallback& callback);
  virtual ~EasyUnlockToggleFlow();

  void Start();

  // OAuth2TokenService::Consumer
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // OAuth2MintTokenFlow::Delegate
  virtual void OnMintTokenSuccess(const std::string& access_token,
                                  int time_to_live) OVERRIDE;
  virtual void OnMintTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnIssueAdviceSuccess(
      const IssueAdviceInfo& issue_advice) OVERRIDE;

 private:
  // Derived OAuth2ApiCallFlow class to make toggle api call after access token
  // is available.
  class ToggleApiCall;

  // Callbacks from ToggleApiCall
  void ReportToggleApiCallResult(bool success);

  Profile* profile_;
  const std::string phone_public_key_;
  const bool toggle_enable_;
  ToggleFlowCallback callback_;

  scoped_ptr<OAuth2TokenService::Request> token_request_;
  scoped_ptr<OAuth2MintTokenFlow> mint_token_flow_;
  scoped_ptr<ToggleApiCall> toggle_api_call_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockToggleFlow);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_TOGGLE_FLOW_H_
