// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_

#include <set>
#include <unordered_map>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/values.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace content {
class WebContents;
}

namespace history {
class HistoryService;
}

class GURL;
class HostContentSettingsMap;

namespace safe_browsing {

class SafeBrowsingDatabaseManager;
class PasswordProtectionRequest;

// UMA metrics
extern const char kPasswordOnFocusRequestOutcomeHistogram[];
extern const char kAnyPasswordEntryRequestOutcomeHistogram[];
extern const char kSyncPasswordEntryRequestOutcomeHistogram[];
extern const char kProtectedPasswordEntryRequestOutcomeHistogram[];
extern const char kSyncPasswordWarningDialogHistogram[];
extern const char kSyncPasswordPageInfoHistogram[];
extern const char kSyncPasswordChromeSettingsHistogram[];

// Manage password protection pings and verdicts. There is one instance of this
// class per profile. Therefore, every PasswordProtectionService instance is
// associated with a unique HistoryService instance and a unique
// HostContentSettingsMap instance.
class PasswordProtectionService : public history::HistoryServiceObserver {
 public:
  using TriggerType = LoginReputationClientRequest::TriggerType;
  using SyncAccountType =
      LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType;

  // The outcome of the request. These values are used for UMA.
  // DO NOT CHANGE THE ORDERING OF THESE VALUES.
  enum RequestOutcome {
    UNKNOWN = 0,
    SUCCEEDED = 1,
    CANCELED = 2,
    TIMEDOUT = 3,
    MATCHED_WHITELIST = 4,
    RESPONSE_ALREADY_CACHED = 5,
    DEPRECATED_NO_EXTENDED_REPORTING = 6,
    DISABLED_DUE_TO_INCOGNITO = 7,
    REQUEST_MALFORMED = 8,
    FETCH_FAILED = 9,
    RESPONSE_MALFORMED = 10,
    SERVICE_DESTROYED = 11,
    DISABLED_DUE_TO_FEATURE_DISABLED = 12,
    DISABLED_DUE_TO_USER_POPULATION = 13,
    URL_NOT_VALID_FOR_REPUTATION_COMPUTING = 14,
    MAX_OUTCOME
  };

  // Enum values indicates if a password protection warning is shown or
  // represents user's action on warnings. These values are used for UMA.
  // DO NOT CHANGE THE ORDERING OF THESE VALUES.
  enum WarningAction {
    // Warning shows up.
    SHOWN = 0,

    // User clicks on "Change Password" button.
    CHANGE_PASSWORD = 1,

    // User clicks on "Ignore" button.
    IGNORE_WARNING = 2,

    // Dialog closed in reaction to change of user state.
    CLOSE = 3,

    // User explicitly mark the site as legitimate.
    MARK_AS_LEGITIMATE = 4,

    MAX_ACTION
  };

  // Type of password protection warning UI.
  enum WarningUIType {
    NOT_USED = 0,
    PAGE_INFO = 1,
    MODAL_DIALOG = 2,
    CHROME_SETTINGS = 3,
    MAX_UI_TYPE
  };

  PasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      history::HistoryService* history_service,
      HostContentSettingsMap* host_content_settings_map);

  ~PasswordProtectionService() override;

  base::WeakPtr<PasswordProtectionService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Looks up |settings| to find the cached verdict response. If verdict is not
  // available or is expired, return VERDICT_TYPE_UNSPECIFIED. Can be called on
  // any thread.
  LoginReputationClientResponse::VerdictType GetCachedVerdict(
      const GURL& url,
      TriggerType trigger_type,
      LoginReputationClientResponse* out_response);

  // Stores |verdict| in |settings| based on its |trigger_type|, |url|,
  // |verdict| and |receive_time|.
  virtual void CacheVerdict(const GURL& url,
                            TriggerType trigger_type,
                            LoginReputationClientResponse* verdict,
                            const base::Time& receive_time);

  // Removes all the expired verdicts from cache.
  void CleanUpExpiredVerdicts();

  // Creates an instance of PasswordProtectionRequest and call Start() on that
  // instance. This function also insert this request object in |requests_| for
  // record keeping.
  void StartRequest(content::WebContents* web_contents,
                    const GURL& main_frame_url,
                    const GURL& password_form_action,
                    const GURL& password_form_frame_url,
                    bool matches_sync_password,
                    const std::vector<std::string>& matching_domains,
                    TriggerType trigger_type,
                    bool password_field_exists);

  virtual void MaybeStartPasswordFieldOnFocusRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const GURL& password_form_action,
      const GURL& password_form_frame_url);

  virtual void MaybeStartProtectedPasswordEntryRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      bool matches_sync_password,
      const std::vector<std::string>& matching_domains,
      bool password_field_exists);

  // Records a Chrome Sync event that sync password reuse was detected.
  virtual void MaybeLogPasswordReuseDetectedEvent(
      content::WebContents* web_contents) = 0;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager();

  // Safe Browsing backend cannot get a reliable reputation of a URL if
  // (1) URL is not valid
  // (2) URL doesn't have http or https scheme
  // (3) It maps to a local host.
  // (4) Its hostname is an IP Address in an IANA-reserved range.
  // (5) Its hostname is a not-yet-assigned by ICANN gTLD.
  // (6) Its hostname is a dotless domain.
  static bool CanGetReputationOfURL(const GURL& url);

  // Records user action to corresponding UMA histograms.
  void RecordWarningAction(WarningUIType ui_type, WarningAction action);

  // Called when user close warning UI or navigate away.
  void OnWarningDone(content::WebContents* web_contents,
                     WarningUIType ui_type,
                     WarningAction action);

  // Shows modal warning dialog on the current |web_contents| and pass the
  // |verdict_token| to callback of this dialog.
  virtual void ShowModalWarning(content::WebContents* web_contents,
                                const std::string& verdict_token) {}

  // Record UMA stats and trigger event logger when warning UI is shown.
  virtual void OnWarningShown(content::WebContents* web_contents,
                              WarningUIType ui_type);

  // If we want to show softer warnings based on Finch parameters.
  static bool ShouldShowSofterWarning();

  virtual void UpdateSecurityState(safe_browsing::SBThreatType threat_type,
                                   content::WebContents* web_contents) {}

  // Log the |reason| to several UMA metrics, depending on the value
  // of |matches_sync_password|.
  static void LogPasswordEntryRequestOutcome(RequestOutcome reason,
                                             bool matches_sync_password);

 protected:
  friend class PasswordProtectionRequest;
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest, VerifyCanSendPing);

  // Chrome can send password protection ping if it is allowed by Finch config
  // and if Safe Browsing can compute reputation of |main_frame_url| (e.g.
  // Safe Browsing is not able to compute reputation of a private IP or
  // a local host). |matches_sync_password| is used for UMA metric recording.
  bool CanSendPing(const base::Feature& feature,
                   const GURL& main_frame_url,
                   bool matches_sync_password);

  // Called by a PasswordProtectionRequest instance when it finishes to remove
  // itself from |requests_|.
  virtual void RequestFinished(
      PasswordProtectionRequest* request,
      bool already_cached,
      std::unique_ptr<LoginReputationClientResponse> response);

  // Cancels all requests in |requests_|, empties it, and releases references to
  // the requests.
  void CancelPendingRequests();

  // Gets the total number of verdicts of the specified |trigger_type| we cached
  // for this profile. This counts both expired and active verdicts.
  virtual int GetStoredVerdictCount(TriggerType trigger_type);

  scoped_refptr<net::URLRequestContextGetter> request_context_getter() {
    return request_context_getter_;
  }

  // Returns the URL where PasswordProtectionRequest instances send requests.
  static GURL GetPasswordProtectionRequestUrl();

  // Gets the request timeout in milliseconds.
  static int GetRequestTimeoutInMS();

  // Obtains referrer chain of |event_url| and |event_tab_id| and adds this
  // info into |frame|.
  virtual void FillReferrerChain(
      const GURL& event_url,
      int event_tab_id,  // -1 if tab id is not available.
      LoginReputationClientRequest::Frame* frame) = 0;

  void FillUserPopulation(TriggerType trigger_type,
                          LoginReputationClientRequest* request_proto);

  virtual bool IsExtendedReporting() = 0;

  virtual bool IsIncognito() = 0;

  virtual bool IsPingingEnabled(const base::Feature& feature,
                                RequestOutcome* reason) = 0;

  virtual bool IsHistorySyncEnabled() = 0;

  // Gets the type of sync account associated with current profile or
  // |NOT_SIGNED_IN|.
  virtual SyncAccountType GetSyncAccountType() = 0;

  // Records a Chrome Sync event for the result of the URL reputation lookup
  // if the user enters their sync password on a website.
  virtual void MaybeLogPasswordReuseLookupEvent(
      content::WebContents* web_contents,
      PasswordProtectionService::RequestOutcome,
      const LoginReputationClientResponse*) = 0;

  void CheckCsdWhitelistOnIOThread(const GURL& url, bool* check_result);

  HostContentSettingsMap* content_settings() const { return content_settings_; }

 private:
  friend class PasswordProtectionServiceTest;
  friend class TestPasswordProtectionService;
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestParseInvalidVerdictEntry);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestParseValidVerdictEntry);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestPathVariantsMatchCacheExpression);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestRemoveCachedVerdictOnURLsDeleted);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestCleanUpExpiredVerdict);

  // Overridden from history::HistoryServiceObserver.
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Posted to UI thread by OnURLsDeleted(..). This function cleans up password
  // protection content settings related to deleted URLs.
  void RemoveContentSettingsOnURLsDeleted(bool all_history,
                                          const history::URLRows& deleted_rows);

  // Helper function called by RemoveContentSettingsOnURLsDeleted(..). It
  // calculate the number of verdicts of |type| that associate with |url|.
  int GetVerdictCountForURL(const GURL& url, TriggerType type);

  // Remove verdict of |type| from |cache_dictionary|. Return false if no
  // verdict removed, true otherwise.
  bool RemoveExpiredVerdicts(TriggerType type,
                             base::DictionaryValue* cache_dictionary);

  static bool ParseVerdictEntry(base::DictionaryValue* verdict_entry,
                                int* out_verdict_received_time,
                                LoginReputationClientResponse* out_verdict);

  static bool PathVariantsMatchCacheExpression(
      const std::vector<std::string>& generated_paths,
      const std::string& cache_expression_path);

  static bool IsCacheExpired(int cache_creation_time, int cache_duration);

  static void GeneratePathVariantsWithoutQuery(const GURL& url,
                                               std::vector<std::string>* paths);

  static std::string GetCacheExpressionPath(
      const std::string& cache_expression);

  static std::unique_ptr<base::DictionaryValue> CreateDictionaryFromVerdict(
      const LoginReputationClientResponse* verdict,
      const base::Time& receive_time);

  static void RecordNoPingingReason(const base::Feature& feature,
                                    RequestOutcome reason,
                                    bool matches_sync_password);
  // Number of verdict stored for this profile for password on focus pings.
  int stored_verdict_count_password_on_focus_;

  // Number of verdict stored for this profile for protected password entry
  // pings.
  int stored_verdict_count_password_entry_;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // The context we use to issue network requests. This request_context_getter
  // is obtained from SafeBrowsingService so that we can use the Safe Browsing
  // cookie store.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // Set of pending PasswordProtectionRequests.
  std::set<scoped_refptr<PasswordProtectionRequest>> requests_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;

  // Content settings map associated with this instance.
  HostContentSettingsMap* content_settings_;

  // Weakptr can only cancel task if it is posted to the same thread. Therefore,
  // we need CancelableTaskTracker to cancel tasks posted to IO thread.
  base::CancelableTaskTracker tracker_;

  base::WeakPtrFactory<PasswordProtectionService> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(PasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
