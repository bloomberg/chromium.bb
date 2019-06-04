// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_LOGIN_TAB_HELPER_H_
#define CHROME_BROWSER_UI_LOGIN_LOGIN_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class LoginDelegate;
class NavigationHandle;
class WebContents;
}  // namespace content

// LoginTabHelper is responsible for observing navigations that need to trigger
// authentication prompts, showing the login prompt, and handling user-entered
// credentials from the prompt.
class LoginTabHelper : public content::WebContentsObserver,
                       public content::WebContentsUserData<LoginTabHelper> {
 public:
  ~LoginTabHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<LoginTabHelper>;

  explicit LoginTabHelper(content::WebContents* web_contents);

  void HandleCredentials(
      const base::Optional<net::AuthCredentials>& credentials);

  std::unique_ptr<content::LoginDelegate> delegate_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(LoginTabHelper);
};

#endif  // CHROME_BROWSER_UI_LOGIN_LOGIN_TAB_HELPER_H_
