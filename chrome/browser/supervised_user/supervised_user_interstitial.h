// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "components/supervised_user_error_page/supervised_user_error_page.h"
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
  // Interstitial type, used for testing.
  static content::InterstitialPageDelegate::TypeID kTypeForTesting;

  static void Show(content::WebContents* web_contents,
                   const GURL& url,
                   supervised_user_error_page::FilteringBehaviorReason reason,
                   bool initial_page_load,
                   const base::Callback<void(bool)>& callback);

  static std::string GetHTMLContents(
      Profile* profile,
      supervised_user_error_page::FilteringBehaviorReason reason);

 private:
  SupervisedUserInterstitial(
      content::WebContents* web_contents,
      const GURL& url,
      supervised_user_error_page::FilteringBehaviorReason reason,
      bool initial_page_load,
      const base::Callback<void(bool)>& callback);
  ~SupervisedUserInterstitial() override;

  bool Init();

  // InterstitialPageDelegate implementation.
  std::string GetHTMLContents() override;
  void CommandReceived(const std::string& command) override;
  void OnProceed() override;
  void OnDontProceed() override;
  content::InterstitialPageDelegate::TypeID GetTypeForTesting() const override;

  // SupervisedUserServiceObserver implementation.
  void OnURLFilterChanged() override;
  // TODO(treib): Also listen to OnCustodianInfoChanged and update as required.

  void OnAccessRequestAdded(bool success);

  // Returns whether we should now proceed on a previously-blocked URL.
  // Called initially before the interstitial is shown (to catch race
  // conditions), or when the URL filtering prefs change. Note that this does
  // not include the asynchronous online checks, so if those are enabled, then
  // the return value indicates only "allow" or "don't know".
  bool ShouldProceed();

  // Moves away from the page behind the interstitial when not proceeding with
  // the request.
  void MoveAwayFromCurrentPage();

  void DispatchContinueRequest(bool continue_request);

  // Owns the interstitial, which owns us.
  content::WebContents* web_contents_;

  Profile* profile_;

  content::InterstitialPage* interstitial_page_;  // Owns us.

  GURL url_;
  supervised_user_error_page::FilteringBehaviorReason reason_;

  // True if the interstitial was shown while loading a page (with a pending
  // navigation), false if it was shown over an already loaded page.
  // Interstitials behave very differently in those cases.
  bool initial_page_load_;

  base::Callback<void(bool)> callback_;

  base::WeakPtrFactory<SupervisedUserInterstitial> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserInterstitial);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_
