// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "url/gurl.h"

namespace content {
class InterstitialPage;
class WebContents;
}

class Profile;

// Delegate for an interstitial page when a page is blocked for a supervised
// user because it is on a blacklist (in "allow everything" mode) or not on any
// whitelist (in "allow only specified sites" mode).
class SupervisedUserInterstitial : public content::InterstitialPageDelegate,
                                   public SupervisedUserServiceObserver {
 public:
  static void Show(content::WebContents* web_contents,
                   const GURL& url,
                   const base::Callback<void(bool)>& callback);

 private:
  SupervisedUserInterstitial(content::WebContents* web_contents,
                             const GURL& url,
                             const base::Callback<void(bool)>& callback);
  virtual ~SupervisedUserInterstitial();

  bool Init();

  // InterstitialPageDelegate implementation.
  virtual std::string GetHTMLContents() OVERRIDE;
  virtual void CommandReceived(const std::string& command) OVERRIDE;
  virtual void OnProceed() OVERRIDE;
  virtual void OnDontProceed() OVERRIDE;

  // SupervisedUserServiceObserver implementation.
  virtual void OnURLFilterChanged() OVERRIDE;

  // Returns whether the blocked URL is now allowed. Called initially before the
  // interstitial is shown (to catch race conditions), or when the URL filtering
  // prefs change.
  bool ShouldProceed();

  void DispatchContinueRequest(bool continue_request);

  // Owns the interstitial, which owns us.
  content::WebContents* web_contents_;

  Profile* profile_;

  content::InterstitialPage* interstitial_page_;  // Owns us.

  GURL url_;

  base::Callback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserInterstitial);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_
