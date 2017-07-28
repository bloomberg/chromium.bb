// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/browser/url_checker_delegate.h"

namespace safe_browsing {

class SafeBrowsingUIManager;

class UrlCheckerDelegateImpl : public UrlCheckerDelegate {
 public:
  UrlCheckerDelegateImpl(
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<SafeBrowsingUIManager> ui_manager);

 private:
  ~UrlCheckerDelegateImpl() override;

  // Implementation of UrlCheckerDelegate:
  void MaybeDestroyPrerenderContents(
      const base::Callback<content::WebContents*()>& web_contents_getter)
      override;
  // Only uses |resource| and ignores the rest of parameters.
  void StartDisplayingBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      bool is_main_frame,
      bool has_user_gesture) override;
  bool IsUrlWhitelisted(const GURL& url) override;
  const SBThreatTypeSet& GetThreatTypes() override;
  SafeBrowsingDatabaseManager* GetDatabaseManager() override;
  BaseUIManager* GetUIManager() override;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  SBThreatTypeSet threat_types_;

  DISALLOW_COPY_AND_ASSIGN(UrlCheckerDelegateImpl);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_
