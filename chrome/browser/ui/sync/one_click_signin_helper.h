// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_HELPER_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_HELPER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/signin/signin_tracker.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "google_apis/gaia/google_service_auth_error.h"

class GURL;
class ProfileIOData;

namespace content {
class WebContents;
struct PasswordForm;
}

namespace net {
class URLRequest;
}

// Per-tab one-click signin helper.  When a user signs in to a Google service
// and the profile is not yet connected to a Google account, will start the
// process of helping the user connect his profile with one click.  The process
// begins with an infobar and is followed with a confirmation dialog explaining
// more about what this means.
class OneClickSigninHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OneClickSigninHelper>,
      public SigninTracker::Observer,
      public ProfileSyncServiceObserver {
 public:
  // Represents user's decision about sign in process.
  enum AutoAccept {
    // User decision not yet known.  Assume cancel.
    AUTO_ACCEPT_NONE,

    // User has explicitly accepted to sign in.  A bubble is shown with the
    // option to start sync, configure it first, or abort.
    AUTO_ACCEPT_ACCEPTED,

    // User has explicitly accepted to sign in, but wants to configure sync
    // settings before turing it on.
    AUTO_ACCEPT_CONFIGURE,

    // User has explicitly rejected to sign in.  Furthermore, the user does
    // not want to be prompted to see the interstitial again in this profile.
    AUTO_ACCEPT_REJECTED_FOR_PROFILE,

    // This is an explicit sign in from either first run, NTP, wrench menu,
    // or settings page.  The user will be signed in automatically with sync
    // enabled using default settings.
    AUTO_ACCEPT_EXPLICIT
  };

  // Return value of CanOfferOnIOThread().
  enum Offer {
    CAN_OFFER,
    DONT_OFFER,
    IGNORE_REQUEST
  };

  // Argument to CanOffer().
  enum CanOfferFor {
    CAN_OFFER_FOR_ALL,
    CAN_OFFER_FOR_INTERSTITAL_ONLY
  };

  virtual ~OneClickSigninHelper();

  // Returns true if the one-click signin feature can be offered at this time.
  // If |email| is not empty, then the profile is checked to see if it's
  // already connected to a google account or if the user has already rejected
  // one-click sign-in with this email, in which cases a one click signin
  // should not be offered.
  //
  // If |can_offer_for| is |CAN_OFFER_FOR_INTERSTITAL_ONLY|, then only do the
  // checks that would affect the interstitial page.  Otherwise, do the checks
  // that would affect the interstitial and the explicit sign ins.
  //
  // Returns in |error_message_id| an explanation as a string resource ID for
  // why one-clicked cannot be offered.  |error_message_id| is valid only if
  // the return value is false.  If no explanation is needed, |error_message_id|
  // may be null.
  static bool CanOffer(content::WebContents* web_contents,
                       CanOfferFor can_offer_for,
                       const std::string& email,
                       std::string* error_message);

  // Returns true if the one-click signin feature can be offered at this time.
  // It can be offered if the io_data is not in an incognito window and if the
  // origin of |url| is a valid Gaia sign in origin.  This function is meant
  // to called only from the IO thread.
  static Offer CanOfferOnIOThread(net::URLRequest* request,
                                  ProfileIOData* io_data);

  // Initialize a finch experiment for the infobar.
  static void InitializeFieldTrial();

  // Looks for the Google-Accounts-SignIn response header, and if found,
  // tries to display an infobar in the tab contents identified by the
  // child/route id.
  static void ShowInfoBarIfPossible(net::URLRequest* request,
                                    ProfileIOData* io_data,
                                    int child_id,
                                    int route_id);

 private:
  explicit OneClickSigninHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<OneClickSigninHelper>;
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperTest,
                           ShowInfoBarUIThreadIncognito);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperTest,
                           SigninFromWebstoreWithConfigSyncfirst);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest, CanOfferOnIOThread);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CanOfferOnIOThreadIncognito);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CanOfferOnIOThreadNoIOData);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CanOfferOnIOThreadBadURL);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CanOfferOnIOThreadReferrer);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CanOfferOnIOThreadDisabled);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CanOfferOnIOThreadSignedIn);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CanOfferOnIOThreadEmailNotAllowed);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CanOfferOnIOThreadEmailAlreadyUsed);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CreateTestProfileIOData);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CanOfferOnIOThreadWithRejectedEmail);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CanOfferOnIOThreadNoSigninCookies);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninHelperIOTest,
                           CanOfferOnIOThreadDisabledByPolicy);

  // Returns true if the one-click signin feature can be offered at this time.
  // It can be offered if the io_data is not in an incognito window and if the
  // origin of |url| is a valid Gaia sign in origin.  This function is meant
  // to called only from the IO thread.
  static Offer CanOfferOnIOThreadImpl(const GURL& url,
                                      const std::string& referrer,
                                      base::SupportsUserData* request,
                                      ProfileIOData* io_data);

  // The portion of ShowInfoBarIfPossible() that needs to run on the UI thread.
  // |session_index| and |email| are extracted from the Google-Accounts-SignIn
  // header.  |auto_accept| is extracted from the Google-Chrome-SignIn header.
  // |source| is used to determine which of the explicit sign in mechanism is
  // being used.
  //
  // |continue_url| is where Gaia will continue to when the sign in process is
  // done.  For explicit sign ins, this is a URL chrome controls. For one-click
  // sign in, this could be any google property.  This URL is used to know
  // when the sign process is over and to collect infomation from the user
  // entered on the Gaia sign in page (for explicit sign ins).
  static void ShowInfoBarUIThread(const std::string& session_index,
                                  const std::string& email,
                                  AutoAccept auto_accept,
                                  SyncPromoUI::Source source,
                                  const GURL& continue_url,
                                  int child_id,
                                  int route_id);

  void RedirectToNTP(bool show_bubble);
  void RedirectToSignin();

  // Clear all data member of the helper, except for the error.
  void CleanTransientState();

  // Grab Gaia password if available.
  bool OnFormSubmitted(const content::PasswordForm& form);

  // content::WebContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // SigninTracker::Observer override.
  virtual void GaiaCredentialsValid() OVERRIDE;
  virtual void SigninFailed(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void SigninSuccess() OVERRIDE;

  // ProfileSyncServiceObserver.
  virtual void OnStateChanged() OVERRIDE;

  // Information about the account that has just logged in.
  std::string session_index_;
  std::string email_;
  std::string password_;
  AutoAccept auto_accept_;
  SyncPromoUI::Source source_;
  GURL continue_url_;
  // Redirect URL after sync setup is complete.
  GURL redirect_url_;
  std::string error_message_;
  scoped_ptr<SigninTracker> signin_tracker_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninHelper);
};

#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_HELPER_H_
