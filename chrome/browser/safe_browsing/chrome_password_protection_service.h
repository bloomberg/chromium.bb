// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_

#include "components/safe_browsing/password_protection/password_protection_service.h"

class Profile;

namespace safe_browsing {

class SafeBrowsingService;
class SafeBrowsingNavigationObserverManager;

// ChromePasswordProtectionService extends PasswordProtectionService by adding
// access to SafeBrowsingNaivigationObserverManager and Profile.
class ChromePasswordProtectionService : public PasswordProtectionService {
 public:
  ChromePasswordProtectionService(SafeBrowsingService* sb_service,
                                  Profile* profile);

  ~ChromePasswordProtectionService() override;

 protected:
  // PasswordProtectionService overrides.
  // Obtains referrer chain of |event_url| and |event_tab_id| and add this
  // info into |frame|.
  void FillReferrerChain(const GURL& event_url,
                         int event_tab_id,
                         LoginReputationClientRequest::Frame* frame) override;

  bool IsExtendedReporting() override;

  bool IsIncognito() override;

  // Checks if Finch config allows sending pings to Safe Browsing Server.
  // |feature| should be either kLowReputationPinging or
  // kProtectedPasswordEntryPinging.
  bool IsPingingEnabled(const base::Feature& feature) override;

  // If user enabled history syncing.
  bool IsHistorySyncEnabled() override;

  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyFinchControlForLowReputationPingSBEROnlyNoIncognito);
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyFinchControlForLowReputationPingSBERAndHistorySyncNoIncognito);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyFinchControlForLowReputationPingAll);
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyFinchControlForLowReputationPingAllButNoIncognito);

 private:
  friend class MockChromePasswordProtectionService;
  // Default constructor used for tests only.
  ChromePasswordProtectionService();
  // Profile associated with this instance.
  Profile* profile_;
  scoped_refptr<SafeBrowsingNavigationObserverManager>
      navigation_observer_manager_;
  DISALLOW_COPY_AND_ASSIGN(ChromePasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
