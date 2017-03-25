// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_

#include <unordered_set>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/safe_browsing/csd.pb.h"
#include "net/url_request/url_request_context_getter.h"

namespace history {
class HistoryService;
}

class GURL;

namespace safe_browsing {

class SafeBrowsingDatabaseManager;
class PasswordProtectionRequest;

class PasswordProtectionService : history::HistoryServiceObserver {
 public:
  using CheckCsdWhitelistCallback = base::Callback<void(bool)>;

  PasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);

  ~PasswordProtectionService() override;

  // Checks if |url| matches CSD whitelist and record UMA metric accordingly.
  // Currently called by PasswordReuseDetectionManager on UI thread.
  void RecordPasswordReuse(const GURL& url);

  void CheckCsdWhitelistOnIOThread(const GURL& url,
                                   const CheckCsdWhitelistCallback& callback);

  base::WeakPtr<PasswordProtectionService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Looks up |settings| to find the cached verdict response. If verdict is not
  // available or is expired, return VERDICT_TYPE_UNSPECIFIED. Can be called on
  // any thread.
  LoginReputationClientResponse::VerdictType GetCachedVerdict(
      const HostContentSettingsMap* settings,
      const GURL& url,
      LoginReputationClientResponse* out_response);

  // Stores |verdict| in |settings| based on |url|, |verdict| and
  // |receive_time|.
  void CacheVerdict(const GURL& url,
                    LoginReputationClientResponse* verdict,
                    const base::Time& receive_time,
                    HostContentSettingsMap* settings);

  // Creates an instance of PasswordProtectionRequest and call Start() on that
  // instance. This function also insert this request object in |requests_| for
  // record keeping.
  void StartRequest(const GURL& main_frame_url,
                    LoginReputationClientRequest::TriggerType type,
                    bool is_extended_reporting,
                    bool is_incognito);

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
  virtual size_t GetStoredVerdictCount();

  scoped_refptr<net::URLRequestContextGetter> request_context_getter() {
    return request_context_getter_;
  }

  // Returns the URL where PasswordProtectionRequest instances send requests.
  static GURL GetPasswordProtectionRequestUrl();

  // Gets the request timeout in milliseconds.
  static int GetRequestTimeoutInMS();

 protected:
  friend class PasswordProtectionRequest;

  // Increases "PasswordManager.PasswordReuse.MainFrameMatchCsdWhitelist" UMA
  // metric based on input.
  void OnMatchCsdWhiteListResult(bool match_whitelist);

  // Gets HostContentSettingMap for current active profile;
  // TODO(jialiul): make this a pure virtual function when we have a derived
  // class ready in chrome/browser/safe_browsing directory.
  virtual HostContentSettingsMap* GetSettingMapForActiveProfile();

 private:
  friend class PasswordProtectionServiceTest;
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestParseInvalidVerdictEntry);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestParseValidVerdictEntry);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestPathVariantsMatchCacheExpression);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestPathMatchCacheExpressionExactly);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestCleanUpCachedVerdicts);

  // Overridden from history::HistoryServiceObserver.
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Posted to UI thread by OnURLsDeleted(..). This function cleans up password
  // protection content settings related to deleted URLs.
  void RemoveContentSettingsOnURLsDeleted(bool all_history,
                                          const history::URLRows& deleted_rows,
                                          HostContentSettingsMap* setting_map);

  static bool ParseVerdictEntry(base::DictionaryValue* verdict_entry,
                                int* out_verdict_received_time,
                                LoginReputationClientResponse* out_verdict);

  static bool PathMatchCacheExpressionExactly(
      const std::vector<std::string>& generated_paths,
      const std::string& cache_expression_path);

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

  // Stored verdict count for each HostContentSettingsMap.
  std::unordered_map<HostContentSettingsMap*, size_t> stored_verdict_counts_;

  // The context we use to issue network requests. This request_context_getter
  // is obtained from SafeBrowsingService so that we can use the Safe Browsing
  // cookie store.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // Set of pending PasswordProtectionRequests.
  std::unordered_set<std::unique_ptr<PasswordProtectionRequest>> requests_;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  base::WeakPtrFactory<PasswordProtectionService> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(PasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
