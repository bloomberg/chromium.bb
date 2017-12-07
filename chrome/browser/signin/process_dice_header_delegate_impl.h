// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_DELEGATE_IMPL_H_

#include "chrome/browser/signin/dice_response_handler.h"

#include "base/macros.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

class ProcessDiceHeaderDelegateImpl : public ProcessDiceHeaderDelegate,
                                      public content::WebContentsObserver {
 public:
  explicit ProcessDiceHeaderDelegateImpl(content::WebContents* web_contents);
  ~ProcessDiceHeaderDelegateImpl() override = default;

  // ProcessDiceHeaderDelegate:
  void EnableSync(const std::string& account_id) override;
  void HandleTokenExchangeFailure(const std::string& email,
                                  const GoogleServiceAuthError& error) override;

 private:
  // Returns true if sync should be enabled after the user signs in.
  bool ShouldEnableSync();

  Profile* profile_;
  bool should_start_sync_ = false;
  signin_metrics::AccessPoint signin_access_point_ =
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN;
  signin_metrics::Reason signin_reason_ =
      signin_metrics::Reason::REASON_UNKNOWN_REASON;

  DISALLOW_COPY_AND_ASSIGN(ProcessDiceHeaderDelegateImpl);
};

#endif  // CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_DELEGATE_IMPL_H_
