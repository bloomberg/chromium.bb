// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_

#include <set>

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
#include "components/safe_browsing/csd.pb.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace history {
class HistoryService;
}

class GURL;
class HostContentSettingsMap;

namespace safe_browsing {

class SafeBrowsingDatabaseManager;
class PasswordProtectionRequest;

extern const base::Feature kLowReputationPinging;
extern const base::Feature kProtectedPasswordEntryPinging;

// Manage password protection pings and verdicts. There is one instance of this
// class per profile. Therefore, every PasswordProtectionService instance is
// associated with a unique HistoryService instance and a unique
// HostContentSettingsMap instance.
class PasswordProtectionService : public history::HistoryServiceObserver {
 public:
  PasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      history::HistoryService* history_service,
      HostContentSettingsMap* host_content_settings_map);

  ~PasswordProtectionService() override;

  base::WeakPtr<PasswordProtectionService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Checks if |url| matches CSD whitelist and record UMA metric accordingly.
  // Currently called by PasswordReuseDetectionManager on UI thread.
  void RecordPasswordReuse(const GURL& url);

  // Looks up |settings| to find the cached verdict response. If verdict is not
  // available or is expired, return VERDICT_TYPE_UNSPECIFIED. Can be called on
  // any thread.
  LoginReputationClientResponse::VerdictType GetCachedVerdict(
      const GURL& url,
      LoginReputationClientResponse* out_response);

  // Stores |verdict| in |settings| based on |url|, |verdict| and
  // |receive_time|.
  void CacheVerdict(const GURL& url,
                    LoginReputationClientResponse* verdict,
                    const base::Time& receive_time);

  // Creates an instance of PasswordProtectionRequest and call Start() on that
  // instance. This function also insert this request object in |requests_| for
  // record keeping.
  void StartRequest(const GURL& main_frame_url,
                    const GURL& password_form_action,
                    const GURL& password_form_frame_url,
                    LoginReputationClientRequest::TriggerType type);

  virtual void MaybeStartLowReputationRequest(
      const GURL& main_frame_url,
      const GURL& password_form_action,
      const GURL& password_form_frame_url);

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager();

 protected:
  friend class PasswordProtectionRequest;

  // Called by a PasswordProtectionRequest instance when it finishes to remove
  // itself from |requests_|.
  virtual void RequestFinished(
      PasswordProtectionRequest* request,
      std::unique_ptr<LoginReputationClientResponse> response);

  // Cancels all requests in |requests_|, empties it, and releases references to
  // the requests.
  void CancelPendingRequests();

  // Gets the total number of verdict (no matter expired or not) we cached for
  // current active profile.
  virtual int GetStoredVerdictCount();

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

  void FillUserPopulation(
      const LoginReputationClientRequest::TriggerType& request_type,
      LoginReputationClientRequest* request_proto);

  virtual bool IsExtendedReporting() = 0;

  virtual bool IsIncognito() = 0;

  virtual bool IsPingingEnabled(const base::Feature& feature) = 0;

  virtual bool IsHistorySyncEnabled() = 0;

  void CheckCsdWhitelistOnIOThread(const GURL& url, bool* check_result);

  // Increases "PasswordManager.PasswordReuse.MainFrameMatchCsdWhitelist" UMA
  // metric based on input.
  void OnMatchCsdWhiteListResult(const bool* match_whitelist);

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
                           TestCleanUpCachedVerdicts);

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

  // Number of verdict stored for this profile.
  int stored_verdict_count_;

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
