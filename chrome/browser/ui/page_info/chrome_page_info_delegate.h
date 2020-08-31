// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_
#define CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_

#include "build/build_config.h"
#include "components/page_info/page_info_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class Profile;
class StatefulSSLHostStateDelegate;

namespace content_settings {
class TabSpecificContentSettings;
}

namespace permissions {
class ChooserContextBase;
class PermissionDecisionAutoBlocker;
}  // namespace permissions

namespace safe_browsing {
class PasswordProtectionService;
class ChromePasswordProtectionService;
}  // namespace safe_browsing

class ChromePageInfoDelegate : public PageInfoDelegate {
 public:
  explicit ChromePageInfoDelegate(content::WebContents* web_contents);
  ~ChromePageInfoDelegate() override = default;

  void SetSecurityStateForTests(
      security_state::SecurityLevel security_level,
      security_state::VisibleSecurityState visible_security_state);

  // PageInfoDelegate implementation
  permissions::ChooserContextBase* GetChooserContext(
      ContentSettingsType type) override;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;
  void OnUserActionOnPasswordUi(content::WebContents* web_contents,
                                safe_browsing::WarningAction action) override;
  base::string16 GetWarningDetailText() override;
#endif
  permissions::PermissionResult GetPermissionStatus(
      ContentSettingsType type,
      const GURL& site_url) override;

#if !defined(OS_ANDROID)
  bool CreateInfoBarDelegate() override;
  void ShowSiteSettings(const GURL& site_url) override;
#endif

  permissions::PermissionDecisionAutoBlocker* GetPermissionDecisionAutoblocker()
      override;
  StatefulSSLHostStateDelegate* GetStatefulSSLHostStateDelegate() override;
  HostContentSettingsMap* GetContentSettings() override;
  bool IsContentDisplayedInVrHeadset() override;
  security_state::SecurityLevel GetSecurityLevel() override;
  security_state::VisibleSecurityState GetVisibleSecurityState() override;
  std::unique_ptr<content_settings::TabSpecificContentSettings::Delegate>
  GetTabSpecificContentSettingsDelegate() override;

#if defined(OS_ANDROID)
  const base::string16 GetClientApplicationName() override;
#endif

 private:
  Profile* GetProfile() const;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::ChromePasswordProtectionService*
  GetChromePasswordProtectionService() const;
#endif
  content::WebContents* web_contents_;
  security_state::SecurityLevel security_level_for_tests_;
  security_state::VisibleSecurityState visible_security_state_for_tests_;
  bool security_state_for_tests_set_ = false;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_
