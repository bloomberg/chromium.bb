// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_MODEL_CLIENT_H_
#define CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_MODEL_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "components/security_state/security_state_model.h"
#include "components/security_state/security_state_model_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/WebKit/public/platform/WebSecurityStyle.h"

namespace content {
struct SecurityStyleExplanations;
class NavigationHandle;
class WebContents;
}  // namespace content

// Uses a WebContents to provide a SecurityStateModel with the
// information that it needs to determine the page's security status.
class ChromeSecurityStateModelClient
    : public security_state::SecurityStateModelClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromeSecurityStateModelClient> {
 public:
  ~ChromeSecurityStateModelClient() override;

  void GetSecurityInfo(
      security_state::SecurityStateModel::SecurityInfo* result) const;

  // Called when the NavigationEntry's SSLStatus or other security
  // information changes.
  void VisibleSecurityStateChanged();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Returns the SecurityStyle that should be applied to a WebContents
  // with the given |security_info|. Populates
  // |security_style_explanations| to explain why the returned
  // SecurityStyle was chosen.
  static blink::WebSecurityStyle GetSecurityStyle(
      const security_state::SecurityStateModel::SecurityInfo& security_info,
      content::SecurityStyleExplanations* security_style_explanations);

  // SecurityStateModelClient:
  void GetVisibleSecurityState(
      security_state::SecurityStateModel::VisibleSecurityState* state) override;
  bool UsedPolicyInstalledCertificate() override;
  bool IsOriginSecure(const GURL& url) override;

 private:
  explicit ChromeSecurityStateModelClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ChromeSecurityStateModelClient>;

  content::WebContents* web_contents_;
  std::unique_ptr<security_state::SecurityStateModel> security_state_model_;

  // True if a console has been logged about an omnibox warning that
  // will be shown in future versions of Chrome for insecure HTTP
  // pages. This message should only be logged once per main-frame
  // navigation.
  bool logged_http_warning_on_current_navigation_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSecurityStateModelClient);
};

#endif  // CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_MODEL_CLIENT_H_
