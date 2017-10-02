// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_OBSERVER_IMPL_H_
#define CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_OBSERVER_IMPL_H_

#include "chrome/browser/signin/dice_response_handler.h"

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

class ProcessDiceHeaderObserverImpl : public ProcessDiceHeaderObserver,
                                      public content::WebContentsObserver {
 public:
  explicit ProcessDiceHeaderObserverImpl(content::WebContents* web_contents);
  ~ProcessDiceHeaderObserverImpl() override = default;

  // ProcessDiceHeaderObserver:
  void WillStartRefreshTokenFetch(const std::string& gaia_id,
                                  const std::string& email) override;
  void DidFinishRefreshTokenFetch(const std::string& gaia_id,
                                  const std::string& email) override;

 private:
  bool should_start_sync_;
  DISALLOW_COPY_AND_ASSIGN(ProcessDiceHeaderObserverImpl);
};

#endif  // CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_OBSERVER_IMPL_H_
