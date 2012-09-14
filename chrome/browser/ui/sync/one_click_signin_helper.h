// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_HELPER_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_HELPER_H_

#include <string>

#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace net {
class URLRequest;
}

// Per-tab one-click signin helper.  When a user signs in to a Google service
// and the profile is not yet connected to a Google account, will start the
// process of helping the user connect his profile with one click.  The process
// begins with an infobar and is followed with a confirmation dialog explaining
// more about what this means.
class OneClickSigninHelper : public content::WebContentsObserver {
 public:
  explicit OneClickSigninHelper(content::WebContents* web_contents);
  virtual ~OneClickSigninHelper();

  // Returns true if the one-click signin feature can be offered at this time.
  // It can be offered if the contents is not in an incognito window.  If
  // |check_connected| is true, then the profile is checked to see if it's
  // already connected to a google account or if the user has already rejected
  // one-click sign-in with this email, in which cases a one click signin
  // should not be offered.
  static bool CanOffer(content::WebContents* web_contents,
                       const std::string& email,
                       bool check_connected);

  // Initialize a finch experiment for the infobar.
  static void InitializeFieldTrial();

  // Looks for the Google-Accounts-SignIn response header, and if found,
  // tries to display an infobar in the tab contents identified by the
  // child/route id.
  static void ShowInfoBarIfPossible(net::URLRequest* request,
                                    int child_id,
                                    int route_id);

 private:
  // The portion of ShowInfoBarIfPossible() that needs to run on the UI thread.
  static void ShowInfoBarUIThread(const std::string& session_index,
                                  const std::string& email,
                                  int child_id,
                                  int route_id);

  // content::WebContentsObserver overrides.
  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // Save the email address that we can display the info bar correctly.
  void SaveSessionIndexAndEmail(const std::string& session_index,
                                const std::string& email);

  // Remember the user's password for later use.
  void SavePassword(const std::string& password);

  // Information about the account that has just logged in.
  std::string session_index_;
  std::string email_;
  std::string password_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninHelper);
};

#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_HELPER_H_
