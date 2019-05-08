// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SECURITY_STATE_TAB_HELPER_H_
#define CHROME_BROWSER_SSL_SECURITY_STATE_TAB_HELPER_H_

#include <memory>

#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/platform/web_security_style.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

// Tab helper provides the page's security status. Also logs console warnings
// for private data on insecure pages.
class SecurityStateTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SecurityStateTabHelper> {
 public:
  ~SecurityStateTabHelper() override;

  // See security_state::GetSecurityLevel.
  security_state::SecurityLevel GetSecurityLevel() const;
  std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState() const;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidChangeVisibleSecurityState() override;

 private:
  explicit SecurityStateTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SecurityStateTabHelper>;

  bool UsedPolicyInstalledCertificate() const;
  security_state::MaliciousContentStatus GetMaliciousContentStatus() const;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SecurityStateTabHelper);
};

#endif  // CHROME_BROWSER_SSL_SECURITY_STATE_TAB_HELPER_H_
