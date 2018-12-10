// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_BASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_BASE_H_

#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace web_app {

// Per-tab web app helper. Allows to associate a tab (web page) with a web app
// (or legacy bookmark app).
class WebAppTabHelperBase
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebAppTabHelperBase> {
 public:
  ~WebAppTabHelperBase() override;

  const AppId& app_id() const { return app_id_; }

  // Set app_id on web app installation or tab restore.
  void SetAppId(const AppId& app_id);
  // Clear app_id on web app uninstallation.
  void ResetAppId();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidCloneToNewWebContents(
      content::WebContents* old_web_contents,
      content::WebContents* new_web_contents) override;

 protected:
  // See documentation in WebContentsUserData class comment.
  explicit WebAppTabHelperBase(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WebAppTabHelperBase>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Clone |this| tab helper (preserving a derived type).
  virtual WebAppTabHelperBase* CloneForWebContents(
      content::WebContents* web_contents) const = 0;

  // Gets AppId from derived platform-specific TabHelper and updates
  // app_id_ with it.
  virtual AppId GetAppId(const GURL& url) = 0;

 private:
  // WebApp associated with this tab. Empty string if no app associated.
  AppId app_id_;

  DISALLOW_COPY_AND_ASSIGN(WebAppTabHelperBase);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_BASE_H_
