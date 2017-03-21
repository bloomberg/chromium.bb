// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/safe_browsing/csd.pb.h"

namespace history {
class HistoryService;
}

class GURL;

namespace safe_browsing {

class SafeBrowsingDatabaseManager;

class PasswordProtectionService : history::HistoryServiceObserver {
 public:
  explicit PasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager);

  ~PasswordProtectionService() override;

  // Checks if |url| matches CSD whitelist and record UMA metric accordingly.
  // Currently called by PasswordReuseDetectionManager on UI thread.
  void RecordPasswordReuse(const GURL& url);

  base::WeakPtr<PasswordProtectionService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Looks up |settings|, and returns the verdict of |url|. Can be called on any
  // thread. If verdict is not available or is expired, return
  // VERDICT_TYPE_UNSPECIFIED.
  LoginReputationClientResponse::VerdictType GetCachedVerdict(
      const HostContentSettingsMap* settings,
      const GURL& url);

  // Stores |verdict| in |settings| based on |url|, |verdict| and
  // |receive_time|.
  void CacheVerdict(const GURL& url,
                    LoginReputationClientResponse* verdict,
                    const base::Time& receive_time,
                    HostContentSettingsMap* settings);

 protected:
  // Called on UI thread.
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

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  base::WeakPtrFactory<PasswordProtectionService> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(PasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
