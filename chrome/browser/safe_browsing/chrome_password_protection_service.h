// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_

#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "ui/base/ui_features.h"

class PrefService;
class Profile;

namespace content {
class WebContents;
}

namespace safe_browsing {

class SafeBrowsingService;
class SafeBrowsingNavigationObserverManager;
class SafeBrowsingUIManager;
class ChromePasswordProtectionService;

using OnWarningDone =
    base::OnceCallback<void(PasswordProtectionService::WarningAction)>;

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
// Shows the platform-specific password reuse modal dialog.
void ShowPasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    OnWarningDone done_callback);
#endif  // !OS_MACOSX || MAC_VIEWS_BROWSER

// ChromePasswordProtectionService extends PasswordProtectionService by adding
// access to SafeBrowsingNaivigationObserverManager and Profile.
class ChromePasswordProtectionService : public PasswordProtectionService {
 public:
  // Observer is used to coordinate password protection UIs (e.g. modal warning,
  // change password card, etc) in reaction to user events.
  class Observer {
   public:
    // Called when user clicks on the "Change Password" button on
    // chrome://settings page.
    virtual void OnStartingGaiaPasswordChange() {}

    // Called when user completes the Gaia password reset.
    virtual void OnGaiaPasswordChanged() {}

    // Called when user marks the site as legitimate.
    virtual void OnMarkingSiteAsLegitimate(const GURL& url) {}

   protected:
    virtual ~Observer() = default;
  };

  ChromePasswordProtectionService(SafeBrowsingService* sb_service,
                                  Profile* profile);

  ~ChromePasswordProtectionService() override;

  static bool ShouldShowChangePasswordSettingUI(Profile* profile);

  void ShowModalWarning(content::WebContents* web_contents,
                        const std::string& verdict_token) override;

  // Called during the construction of Observer subclass.
  virtual void AddObserver(Observer* observer);

  // Called during the destruction of the observer subclass.
  virtual void RemoveObserver(Observer* observer);

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
  // If not, indicated its reason by modifying |reason|.
  // |feature| should be either kLowReputationPinging or
  // kProtectedPasswordEntryPinging.
  bool IsPingingEnabled(const base::Feature& feature,
                        RequestOutcome* reason) override;

  // If user enabled history syncing.
  bool IsHistorySyncEnabled() override;

  void MaybeLogPasswordReuseDetectedEvent(
      content::WebContents* web_contents) override;

  PasswordProtectionService::SyncAccountType GetSyncAccountType() override;

  void MaybeLogPasswordReuseLookupEvent(
      content::WebContents* web_contents,
      PasswordProtectionService::RequestOutcome outcome,
      const LoginReputationClientResponse* response) override;

  // Update security state for the current |web_contents| based on
  // |threat_type|, such that page info bubble will show appropriate status
  // when user clicks on the security chip.
  void UpdateSecurityState(SBThreatType threat_type,
                           content::WebContents* web_contents) override;

  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyUserPopulationForPasswordOnFocusPing);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyUserPopulationForProtectedPasswordEntryPing);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordReuseUserEventNotRecorded);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordReuseDetectedUserEventRecorded);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordReuseLookupUserEventRecorded);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyGetSyncAccountType);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyUpdateSecurityState);

 private:
  // Gets prefs associated with |profile_|.
  PrefService* GetPrefs();

  // Returns whether the profile is valid and has safe browsing service enabled.
  bool IsSafeBrowsingEnabled();

  std::unique_ptr<sync_pb::UserEventSpecifics> GetUserEventSpecifics(
      content::WebContents* web_contents);

  void LogPasswordReuseLookupResult(
      content::WebContents* web_contents,
      sync_pb::UserEventSpecifics::GaiaPasswordReuse::PasswordReuseLookup::
          LookupResult result);

  void LogPasswordReuseLookupResultWithVerdict(
      content::WebContents* web_contents,
      sync_pb::UserEventSpecifics::GaiaPasswordReuse::PasswordReuseLookup::
          LookupResult result,
      sync_pb::UserEventSpecifics::GaiaPasswordReuse::PasswordReuseLookup::
          ReputationVerdict verdict,
      const std::string& verdict_token);

  friend class MockChromePasswordProtectionService;
  // Constructor used for tests only.
  ChromePasswordProtectionService(
      Profile* profile,
      scoped_refptr<HostContentSettingsMap> content_setting_map,
      scoped_refptr<SafeBrowsingUIManager> ui_manager);

  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  // Profile associated with this instance.
  Profile* profile_;
  scoped_refptr<SafeBrowsingNavigationObserverManager>
      navigation_observer_manager_;
  base::ObserverList<Observer> observer_list_;
  DISALLOW_COPY_AND_ASSIGN(ChromePasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
