// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/password_protection/password_protection_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

// Keys for storing password protection verdict into a DictionaryValue.
static const char kCacheCreationTime[] = "cache_creation_time";
static const char kVerdictProto[] = "verdict_proto";

// Helper function to determine if the given origin matches content settings
// map's patterns.
bool OriginMatchPrimaryPattern(
    const GURL& origin,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern_unused) {
  return ContentSettingsPattern::FromURLNoWildcard(origin) == primary_pattern;
}

// Returns the number of path segments in |cache_expression_path|.
// For example, return 0 for "/", since there is no path after the leading
// slash; return 3 for "/abc/def/gh.html".
size_t GetPathDepth(const std::string& cache_expression_path) {
  return base::SplitString(base::StringPiece(cache_expression_path), "/",
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)
      .size();
}

// Given a URL of either http or https scheme, return its scheme://hostname.
// e.g., "https://www.foo.com:80/bar/test.cgi" -> "http://www.foo.com".
GURL GetHostNameWithHTTPScheme(const GURL& url) {
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  std::string result(url::kHttpScheme);
  result.append(url::kStandardSchemeSeparator).append(url.HostNoBrackets());
  return GURL(result);
}

}  // namespace

PasswordProtectionService::PasswordProtectionService(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager)
    : database_manager_(database_manager), weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PasswordProtectionService::~PasswordProtectionService() {
  weak_factory_.InvalidateWeakPtrs();
}

void PasswordProtectionService::RecordPasswordReuse(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(database_manager_);
  if (!url.is_valid())
    return;

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::MatchCsdWhitelistUrl,
                 database_manager_, url),
      base::Bind(&PasswordProtectionService::OnMatchCsdWhiteListResult,
                 GetWeakPtr()));
}

LoginReputationClientResponse::VerdictType
PasswordProtectionService::GetCachedVerdict(
    const HostContentSettingsMap* settings,
    const GURL& url) {
  // TODO(jialiul): Add UMA metrics to track if verdict is available, not
  // available, or expired.
  DCHECK(settings);
  if (!url.is_valid()) {
    return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
  }

  GURL hostname = GetHostNameWithHTTPScheme(url);
  std::unique_ptr<base::DictionaryValue> verdict_dictionary =
      base::DictionaryValue::From(settings->GetWebsiteSetting(
          hostname, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
          std::string(), nullptr));
  // Return early if there is no verdict cached for this origin.
  if (!verdict_dictionary.get() || verdict_dictionary->empty()) {
    return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
  }

  std::vector<std::string> paths;
  GeneratePathVariantsWithoutQuery(url, &paths);
  size_t max_path_depth = 0U;
  LoginReputationClientResponse::VerdictType most_matching_verdict =
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
  // For all the verdicts of the same origin, we key them by |cache_expression|.
  // Its corresponding value is a DictionaryValue contains its creation time and
  // the serialized verdict proto.
  for (base::DictionaryValue::Iterator it(*verdict_dictionary.get());
       !it.IsAtEnd(); it.Advance()) {
    base::DictionaryValue* verdict_entry = nullptr;
    CHECK(verdict_dictionary->GetDictionaryWithoutPathExpansion(
        it.key() /* cache_expression */, &verdict_entry));
    int verdict_received_time;
    LoginReputationClientResponse verdict;
    CHECK(ParseVerdictEntry(verdict_entry, &verdict_received_time, &verdict));
    // Since password protection content settings are keyed by origin, we only
    // need to compare the path part of the cache_expression and the given url.
    std::string cache_expression_path =
        GetCacheExpressionPath(verdict.cache_expression());

    if (verdict.cache_expression_exact_match()) {
      if (PathMatchCacheExpressionExactly(paths, cache_expression_path)) {
        return IsCacheExpired(verdict_received_time,
                              verdict.cache_duration_sec())
                   ? LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED
                   : verdict.verdict_type();
      }
    } else {
      // If it doesn't require exact match, we need to fine the most specific
      // match.
      size_t path_depth = GetPathDepth(cache_expression_path);
      if (path_depth > max_path_depth &&
          PathVariantsMatchCacheExpression(paths, cache_expression_path) &&
          !IsCacheExpired(verdict_received_time,
                          verdict.cache_duration_sec())) {
        max_path_depth = path_depth;
        most_matching_verdict = verdict.verdict_type();
      }
    }
  }
  return most_matching_verdict;
}

void PasswordProtectionService::CacheVerdict(
    const GURL& url,
    LoginReputationClientResponse* verdict,
    const base::Time& receive_time,
    HostContentSettingsMap* settings) {
  DCHECK(verdict);

  GURL hostname = GetHostNameWithHTTPScheme(url);
  std::unique_ptr<base::DictionaryValue> verdict_dictionary =
      base::DictionaryValue::From(settings->GetWebsiteSetting(
          hostname, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
          std::string(), nullptr));

  if (!verdict_dictionary.get())
    verdict_dictionary = base::MakeUnique<base::DictionaryValue>();

  std::unique_ptr<base::DictionaryValue> verdict_entry =
      CreateDictionaryFromVerdict(verdict, receive_time);
  // If same cache_expression is already in this verdict_dictionary, we simply
  // override it.
  verdict_dictionary->SetWithoutPathExpansion(verdict->cache_expression(),
                                              std::move(verdict_entry));
  settings->SetWebsiteSettingDefaultScope(
      hostname, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
      std::string(), std::move(verdict_dictionary));
}

void PasswordProtectionService::OnMatchCsdWhiteListResult(
    bool match_whitelist) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.PasswordReuse.MainFrameMatchCsdWhitelist",
      match_whitelist);
}

HostContentSettingsMap*
PasswordProtectionService::GetSettingMapForActiveProfile() {
  // TODO(jialiul): Make this a pure virtual function when we have a derived
  // class ready in chrome/browser/safe_browsing directory.
  return nullptr;
}

void PasswordProtectionService::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  HostContentSettingsMap* setting_map = GetSettingMapForActiveProfile();
  if (!setting_map)
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PasswordProtectionService::RemoveContentSettingsOnURLsDeleted,
                 GetWeakPtr(), all_history, deleted_rows, setting_map));
}

void PasswordProtectionService::RemoveContentSettingsOnURLsDeleted(
    bool all_history,
    const history::URLRows& deleted_rows,
    HostContentSettingsMap* setting_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (all_history) {
    setting_map->ClearSettingsForOneType(
        CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION);
    return;
  }

  // For now, if a URL is deleted from history, we simply remove all the
  // cached verdicts of the same origin. This is a pretty aggressive deletion.
  // We might revisit this logic later to decide if we want to only delete the
  // cached verdict whose cache expression matches this URL.
  for (const history::URLRow& row : deleted_rows) {
    setting_map->ClearSettingsForOneTypeWithPredicate(
        CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
        base::Bind(&OriginMatchPrimaryPattern, row.url().GetOrigin()));
  }
}

// static
bool PasswordProtectionService::ParseVerdictEntry(
    base::DictionaryValue* verdict_entry,
    int* out_verdict_received_time,
    LoginReputationClientResponse* out_verdict) {
  base::Value* binary_value = nullptr;
  bool result = verdict_entry && out_verdict &&
                verdict_entry->GetInteger(kCacheCreationTime,
                                          out_verdict_received_time) &&
                verdict_entry->Get(kVerdictProto, &binary_value);
  if (!result)
    return false;
  DCHECK(result);
  DCHECK_EQ(base::Value::Type::BINARY, binary_value->type());
  const auto blob = binary_value->GetBlob();
  const std::string serialized_verdict_proto(blob.begin(), blob.end());
  return out_verdict->ParseFromString(serialized_verdict_proto);
}

bool PasswordProtectionService::PathMatchCacheExpressionExactly(
    const std::vector<std::string>& generated_paths,
    const std::string& cache_expression_path) {
  size_t cache_expression_path_depth = GetPathDepth(cache_expression_path);
  if (generated_paths.size() <= cache_expression_path_depth) {
    return false;
  }
  std::string canonical_path = generated_paths.back();
  size_t last_slash_pos = canonical_path.find_last_of("/");
  DCHECK_NE(std::string::npos, last_slash_pos);
  return canonical_path.substr(0, last_slash_pos + 1) == cache_expression_path;
}

bool PasswordProtectionService::PathVariantsMatchCacheExpression(
    const std::vector<std::string>& generated_paths,
    const std::string& cache_expression_path) {
  for (const auto& path : generated_paths) {
    if (cache_expression_path == path) {
      return true;
    }
  }
  return false;
}

bool PasswordProtectionService::IsCacheExpired(int cache_creation_time,
                                               int cache_duration) {
  // TODO(jialiul): For now, we assume client's clock is accurate or almost
  // accurate. Need some logic to handle cases where client's clock is way
  // off.
  return base::Time::Now().ToDoubleT() >
         static_cast<double>(cache_creation_time + cache_duration);
}

// Generate path variants of the given URL.
void PasswordProtectionService::GeneratePathVariantsWithoutQuery(
    const GURL& url,
    std::vector<std::string>* paths) {
  std::string canonical_path;
  V4ProtocolManagerUtil::CanonicalizeUrl(url, nullptr, &canonical_path,
                                         nullptr);
  V4ProtocolManagerUtil::GeneratePathVariantsToCheck(canonical_path,
                                                     std::string(), paths);
}

// Return the path of the cache expression. e.g.:
// "www.google.com"     -> "/"
// "www.google.com/abc" -> "/abc/"
// "foo.com/foo/bar/"  -> "/foo/bar/"
std::string PasswordProtectionService::GetCacheExpressionPath(
    const std::string& cache_expression) {
  DCHECK(!cache_expression.empty());
  std::string out_put(cache_expression);
  // Append a trailing slash if needed.
  if (out_put[out_put.length() - 1] != '/')
    out_put.append("/");

  size_t first_slash_pos = out_put.find_first_of("/");
  DCHECK_NE(std::string::npos, first_slash_pos);
  return out_put.substr(first_slash_pos);
}

// Convert a LoginReputationClientResponse proto into a DictionaryValue.
std::unique_ptr<base::DictionaryValue>
PasswordProtectionService::CreateDictionaryFromVerdict(
    const LoginReputationClientResponse* verdict,
    const base::Time& receive_time) {
  std::unique_ptr<base::DictionaryValue> result =
      base::MakeUnique<base::DictionaryValue>();
  result->SetInteger(kCacheCreationTime,
                     static_cast<int>(receive_time.ToDoubleT()));
  // Because DictionaryValue cannot take non-UTF8 string, we need to store
  // serialized proto in its allowed binary format instead.
  const std::string serialized_proto(verdict->SerializeAsString());
  const std::vector<char> verdict_blob(serialized_proto.begin(),
                                       serialized_proto.end());
  std::unique_ptr<base::Value> binary_value =
      base::MakeUnique<base::Value>(verdict_blob);
  DCHECK_EQ(base::Value::Type::BINARY, binary_value->type());
  result->Set(kVerdictProto, std::move(binary_value));
  return result;
}

}  // namespace safe_browsing
