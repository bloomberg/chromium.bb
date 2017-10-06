// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_

#include <map>

#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "ui/base/ui_features.h"
#include "url/origin.h"

struct AccountInfo;
class PrefChangeRegistrar;
class PrefService;
class PrefChangeRegistrar;
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
using url::Origin;

// Shows the platform-specific password reuse modal dialog.
void ShowPasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    OnWarningDone done_callback);

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
    virtual void OnStartingGaiaPasswordChange() = 0;

    // Called when user completes the Gaia password reset.
    virtual void OnGaiaPasswordChanged() = 0;

    // Called when user marks the site as legitimate.
    virtual void OnMarkingSiteAsLegitimate(const GURL& url) = 0;

    // Only to be used by tests. Subclasses must override to manually call the
    // respective button click handler.
    virtual void InvokeActionForTesting(
        ChromePasswordProtectionService::WarningAction action) = 0;

    // Only to be used by tests.
    virtual ChromePasswordProtectionService::WarningUIType
    GetObserverType() = 0;

   protected:
    virtual ~Observer() = default;
  };

  ChromePasswordProtectionService(SafeBrowsingService* sb_service,
                                  Profile* profile);

  ~ChromePasswordProtectionService() override;

  static ChromePasswordProtectionService* GetPasswordProtectionService(
      Profile* profile);

  static bool ShouldShowChangePasswordSettingUI(Profile* profile);

  void ShowModalWarning(content::WebContents* web_contents,
                        const std::string& verdict_token) override;

  // Called when user interacts with password protection UIs.
  void OnUserAction(content::WebContents* web_contents,
                    PasswordProtectionService::WarningUIType ui_type,
                    PasswordProtectionService::WarningAction action) override;

  // Called during the construction of Observer subclass.
  virtual void AddObserver(Observer* observer);

  // Called during the destruction of the observer subclass.
  virtual void RemoveObserver(Observer* observer);

  // Starts collecting threat details if user has extended reporting enabled and
  // is not in incognito mode.
  void MaybeStartThreatDetailsCollection(content::WebContents* web_contents,
                                         const std::string& token);

  // Sends threat details if user has extended reporting enabled and is not in
  // incognito mode.
  void MaybeFinishCollectingThreatDetails(content::WebContents* web_contents,
                                          bool did_proceed);

  const std::map<Origin, int64_t>& unhandled_password_reuses() const {
    return unhandled_password_reuses_;
  }

  // Called when sync user's Gaia password changed.
  void OnGaiaPasswordChanged();

  // If user has clicked through any Safe Browsing interstitial on this given
  // |web_contents|.
  bool UserClickedThroughSBInterstitial(
      content::WebContents* web_contents) override;

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

  // Updates security state for the current |web_contents| based on
  // |threat_type|, such that page info bubble will show appropriate status
  // when user clicks on the security chip.
  void UpdateSecurityState(SBThreatType threat_type,
                           content::WebContents* web_contents) override;

  // Sets |account_info_| based on |profile_|.
  void InitializeAccountInfo();

  // Gets change password URl based on |account_info_|.
  GURL GetChangePasswordURL();

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
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyGetChangePasswordURL);

 private:
  friend class MockChromePasswordProtectionService;
  friend class ChromePasswordProtectionServiceBrowserTest;

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

  // Constructor used for tests only.
  ChromePasswordProtectionService(
      Profile* profile,
      scoped_refptr<HostContentSettingsMap> content_setting_map,
      scoped_refptr<SafeBrowsingUIManager> ui_manager);

  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  TriggerManager* trigger_manager_;
  // Profile associated with this instance.
  Profile* profile_;
  // AccountInfo associated with this |profile_|.
  std::unique_ptr<AccountInfo> account_info_;
  scoped_refptr<SafeBrowsingNavigationObserverManager>
      navigation_observer_manager_;
  base::ObserverList<Observer> observer_list_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  // The map of password reuse origin of top-level frame to navigation ID. These
  // are password reuses that user hasn't chosen to change password, or
  // mark site as legitimate yet.
  std::map<Origin, int64_t> unhandled_password_reuses_;
  DISALLOW_COPY_AND_ASSIGN(ChromePasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
