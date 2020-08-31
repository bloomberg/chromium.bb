// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_REAUTH_TAB_HELPER_H_
#define CHROME_BROWSER_SIGNIN_REAUTH_TAB_HELPER_H_

#include "base/callback.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace signin {

enum class ReauthResult;

// Tab helper class observing navigations within the reauth flow and notifying
// a caller about a flow result.
class ReauthTabHelper : public content::WebContentsUserData<ReauthTabHelper>,
                        public content::WebContentsObserver {
 public:
  using ReauthCallback = base::OnceCallback<void(signin::ReauthResult)>;

  // Creates a new ReauthTabHelper and attaches it to |web_contents|. If an
  // instance is already attached, no replacement happens, just notifies the
  // caller by invoking |callback| with signin::ReauthResult::kCancelled.
  // Initializes a helper with:
  // - |callback| to be called when the reauth flow is complete.
  // - |reauth_url| that should be the final destination of the reauth flow.
  // - |restrict_to_reauth_origin| determines whether the reauth flow can
  //   navigate outside of |reauth_url|'s origin.
  static void CreateForWebContents(content::WebContents* web_contents,
                                   const GURL& reauth_url,
                                   bool restrict_to_reauth_origin,
                                   ReauthCallback callback);

  ReauthTabHelper(const ReauthTabHelper&) = delete;
  ReauthTabHelper& operator=(const ReauthTabHelper&) = delete;

  ~ReauthTabHelper() override;

  // Determines whether the current navigation is allowed within the reauth
  // flow.
  bool ShouldAllowNavigation(content::NavigationHandle* navigation_handle);

  // If |callback_| is not null, calls |callback_| with |result|.
  void CompleteReauth(signin::ReauthResult result);

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<ReauthTabHelper>;
  explicit ReauthTabHelper(content::WebContents* web_contents,
                           const GURL& reauth_url,
                           bool restrict_to_reauth_origin,
                           ReauthCallback callback);

  const GURL reauth_url_;
  const bool restrict_to_reauth_origin_;
  ReauthCallback callback_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_REAUTH_TAB_HELPER_H_
