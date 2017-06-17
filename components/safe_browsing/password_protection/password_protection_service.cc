// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/password_protection/password_protection_service.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "components/safe_browsing_db/whitelist_checker_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"

using content::BrowserThread;
using content::WebContents;
using history::HistoryService;

namespace safe_browsing {

namespace {

// Keys for storing password protection verdict into a DictionaryValue.
const char kCacheCreationTime[] = "cache_creation_time";
const char kVerdictProto[] = "verdict_proto";
const int kRequestTimeoutMs = 10000;
const char kPasswordProtectionRequestUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/login";

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

const base::Feature kPasswordFieldOnFocusPinging{
    "PasswordFieldOnFocusPinging", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kProtectedPasswordEntryPinging{
    "ProtectedPasswordEntryPinging", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPasswordProtectionInterstitial{
    "PasswordProtectionInterstitial", base::FEATURE_DISABLED_BY_DEFAULT};

const char kPasswordOnFocusRequestOutcomeHistogramName[] =
    "PasswordProtection.RequestOutcome.PasswordFieldOnFocus";
const char kPasswordEntryRequestOutcomeHistogramName[] =
    "PasswordProtection.RequestOutcome.ProtectedPasswordEntry";

PasswordProtectionService::PasswordProtectionService(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    HistoryService* history_service,
    HostContentSettingsMap* host_content_settings_map)
    : stored_verdict_count_(-1),
      database_manager_(database_manager),
      request_context_getter_(request_context_getter),
      history_service_observer_(this),
      content_settings_(host_content_settings_map),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (history_service)
    history_service_observer_.Add(history_service);
}

PasswordProtectionService::~PasswordProtectionService() {
  tracker_.TryCancelAll();
  CancelPendingRequests();
  history_service_observer_.RemoveAll();
  weak_factory_.InvalidateWeakPtrs();
}

bool PasswordProtectionService::CanGetReputationOfURL(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return false;

  const std::string& hostname = url.HostNoBrackets();
  return !net::IsLocalhost(hostname) && !net::IsHostnameNonUnique(hostname) &&
         hostname.find('.') != std::string::npos;
}

LoginReputationClientResponse::VerdictType
PasswordProtectionService::GetCachedVerdict(
    const GURL& url,
    LoginReputationClientResponse* out_response) {
  if (!url.is_valid())
    return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;

  DCHECK(content_settings_);

  GURL hostname = GetHostNameWithHTTPScheme(url);
  std::unique_ptr<base::DictionaryValue> verdict_dictionary =
      base::DictionaryValue::From(content_settings_->GetWebsiteSetting(
          hostname, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
          std::string(), nullptr));
  // Return early if there is no verdict cached for this origin.
  if (!verdict_dictionary.get() || verdict_dictionary->empty())
    return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;

  std::vector<std::string> paths;
  GeneratePathVariantsWithoutQuery(url, &paths);
  int max_path_depth = -1;
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

    // Finds the most specific match.
    int path_depth = static_cast<int>(GetPathDepth(cache_expression_path));
    if (path_depth > max_path_depth &&
        PathVariantsMatchCacheExpression(paths, cache_expression_path)) {
      max_path_depth = path_depth;
      // If the most matching verdict is expired, set the result to
      // VERDICT_TYPE_UNSPECIFIED.
      most_matching_verdict =
          IsCacheExpired(verdict_received_time, verdict.cache_duration_sec())
              ? LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED
              : verdict.verdict_type();
      out_response->CopyFrom(verdict);
    }
  }
  return most_matching_verdict;
}

void PasswordProtectionService::CacheVerdict(
    const GURL& url,
    LoginReputationClientResponse* verdict,
    const base::Time& receive_time) {
  DCHECK(verdict);
  DCHECK(content_settings_);

  GURL hostname = GetHostNameWithHTTPScheme(url);
  std::unique_ptr<base::DictionaryValue> verdict_dictionary =
      base::DictionaryValue::From(content_settings_->GetWebsiteSetting(
          hostname, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
          std::string(), nullptr));

  if (!verdict_dictionary.get())
    verdict_dictionary = base::MakeUnique<base::DictionaryValue>();

  std::unique_ptr<base::DictionaryValue> verdict_entry =
      CreateDictionaryFromVerdict(verdict, receive_time);

  // Increases stored verdict count if we haven't seen this cache expression
  // before.
  if (!verdict_dictionary->HasKey(verdict->cache_expression()))
    stored_verdict_count_ = GetStoredVerdictCount() + 1;
  // If same cache_expression is already in this verdict_dictionary, we simply
  // override it.
  verdict_dictionary->SetWithoutPathExpansion(verdict->cache_expression(),
                                              std::move(verdict_entry));
  content_settings_->SetWebsiteSettingDefaultScope(
      hostname, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
      std::string(), std::move(verdict_dictionary));
}

void PasswordProtectionService::CleanUpExpiredVerdicts() {
  DCHECK(content_settings_);
  if (GetStoredVerdictCount() <= 0)
    return;

  ContentSettingsForOneType password_protection_settings;
  content_settings_->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(),
      &password_protection_settings);

  for (const ContentSettingPatternSource& source :
       password_protection_settings) {
    GURL primary_pattern_url = GURL(source.primary_pattern.ToString());
    // Find all verdicts associated with this origin.
    std::unique_ptr<base::DictionaryValue> verdict_dictionary =
        base::DictionaryValue::From(content_settings_->GetWebsiteSetting(
            primary_pattern_url, GURL(),
            CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(), nullptr));
    std::vector<std::string> expired_keys;
    for (base::DictionaryValue::Iterator it(*verdict_dictionary.get());
         !it.IsAtEnd(); it.Advance()) {
      base::DictionaryValue* verdict_entry = nullptr;
      CHECK(verdict_dictionary->GetDictionaryWithoutPathExpansion(
          it.key(), &verdict_entry));
      int verdict_received_time;
      LoginReputationClientResponse verdict;
      CHECK(ParseVerdictEntry(verdict_entry, &verdict_received_time, &verdict));

      if (IsCacheExpired(verdict_received_time, verdict.cache_duration_sec())) {
        // Since DictionaryValue::Iterator cannot be used to modify the
        // dictionary, we record the keys of expired verdicts in |expired_keys|
        // and remove them in the next for-loop.
        expired_keys.push_back(it.key());
      }
    }

    for (const std::string& key : expired_keys) {
      verdict_dictionary->RemoveWithoutPathExpansion(key, nullptr);
      stored_verdict_count_--;
    }

    if (verdict_dictionary->size() == 0u) {
      content_settings_->ClearSettingsForOneTypeWithPredicate(
          CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, base::Time(),
          base::Bind(&OriginMatchPrimaryPattern, primary_pattern_url));
    } else if (expired_keys.size() > 0u) {
      // Set the website setting of this origin with the updated
      // |verdict_diectionary|.
      content_settings_->SetWebsiteSettingDefaultScope(
          primary_pattern_url, GURL(),
          CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(),
          std::move(verdict_dictionary));
    }
  }
}

void PasswordProtectionService::StartRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url,
    const std::string& saved_domain,
    LoginReputationClientRequest::TriggerType type,
    bool password_field_exists) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<PasswordProtectionRequest> request(
      new PasswordProtectionRequest(
          web_contents, main_frame_url, password_form_action,
          password_form_frame_url, saved_domain, type, password_field_exists,
          this, GetRequestTimeoutInMS()));
  DCHECK(request);
  request->Start();
  requests_.insert(std::move(request));
}

void PasswordProtectionService::MaybeStartPasswordFieldOnFocusRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (CanSendPing(kPasswordFieldOnFocusPinging, main_frame_url)) {
    StartRequest(web_contents, main_frame_url, password_form_action,
                 password_form_frame_url,
                 std::string(), /* saved_domain: not used for this type */
                 LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  }
}

void PasswordProtectionService::MaybeStartProtectedPasswordEntryRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const std::string& saved_domain,
    bool password_field_exists) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (CanSendPing(kProtectedPasswordEntryPinging, main_frame_url)) {
    StartRequest(web_contents, main_frame_url, GURL(), GURL(), saved_domain,
                 LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 password_field_exists);
  }
}

bool PasswordProtectionService::CanSendPing(const base::Feature& feature,
                                            const GURL& main_frame_url) {
  RequestOutcome request_outcome = URL_NOT_VALID_FOR_REPUTATION_COMPUTING;
  if (IsPingingEnabled(kPasswordFieldOnFocusPinging, &request_outcome) &&
      CanGetReputationOfURL(main_frame_url)) {
    return true;
  }
  RecordNoPingingReason(feature, request_outcome);
  return false;
}

void PasswordProtectionService::RequestFinished(
    PasswordProtectionRequest* request,
    bool already_cached,
    std::unique_ptr<LoginReputationClientResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(request);

  if (response) {
    if (!already_cached) {
      CacheVerdict(request->main_frame_url(), response.get(),
                   base::Time::Now());
    }

    if (request->request_type() ==
            LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE &&
        response->verdict_type() == LoginReputationClientResponse::PHISHING &&
        base::FeatureList::IsEnabled(kPasswordProtectionInterstitial)) {
      ShowPhishingInterstitial(request->main_frame_url(),
                               response->verdict_token(),
                               request->web_contents());
    }
  }

  // Finished processing this request. Remove it from pending list.
  for (auto it = requests_.begin(); it != requests_.end(); it++) {
    if (it->get() == request) {
      requests_.erase(it);
      break;
    }
  }
}

void PasswordProtectionService::CancelPendingRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto it = requests_.begin(); it != requests_.end();) {
    // We need to advance the iterator before we cancel because canceling
    // the request will invalidate it when RequestFinished is called.
    PasswordProtectionRequest* request = it->get();
    it++;
    request->Cancel(false);
  }
  DCHECK(requests_.empty());
}

scoped_refptr<SafeBrowsingDatabaseManager>
PasswordProtectionService::database_manager() {
  return database_manager_;
}

GURL PasswordProtectionService::GetPasswordProtectionRequestUrl() {
  GURL url(kPasswordProtectionRequestUrl);
  std::string api_key = google_apis::GetAPIKey();
  DCHECK(!api_key.empty());
  return url.Resolve("?key=" + net::EscapeQueryParamValue(api_key, true));
}

int PasswordProtectionService::GetStoredVerdictCount() {
  DCHECK(content_settings_);
  // If we have already computed this, return its value.
  if (stored_verdict_count_ >= 0)
    return stored_verdict_count_;

  ContentSettingsForOneType password_protection_settings;
  content_settings_->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(),
      &password_protection_settings);
  stored_verdict_count_ = 0;
  if (password_protection_settings.empty())
    return 0;

  for (const ContentSettingPatternSource& source :
       password_protection_settings) {
    std::unique_ptr<base::DictionaryValue> verdict_dictionary =
        base::DictionaryValue::From(content_settings_->GetWebsiteSetting(
            GURL(source.primary_pattern.ToString()), GURL(),
            CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(), nullptr));
    if (verdict_dictionary.get() && !verdict_dictionary->empty())
      stored_verdict_count_ += static_cast<int>(verdict_dictionary->size());
  }
  return stored_verdict_count_;
}

int PasswordProtectionService::GetRequestTimeoutInMS() {
  return kRequestTimeoutMs;
}

void PasswordProtectionService::FillUserPopulation(
    const LoginReputationClientRequest::TriggerType& request_type,
    LoginReputationClientRequest* request_proto) {
  ChromeUserPopulation* user_population = request_proto->mutable_population();
  user_population->set_user_population(
      IsExtendedReporting() ? ChromeUserPopulation::EXTENDED_REPORTING
                            : ChromeUserPopulation::SAFE_BROWSING);
  user_population->set_is_history_sync_enabled(IsHistorySyncEnabled());

  base::FieldTrial* low_reputation_field_trial =
      base::FeatureList::GetFieldTrial(kPasswordFieldOnFocusPinging);
  if (low_reputation_field_trial) {
    user_population->add_finch_active_groups(
        low_reputation_field_trial->trial_name() + "|" +
        low_reputation_field_trial->group_name());
  }
  base::FieldTrial* password_entry_field_trial =
      base::FeatureList::GetFieldTrial(kProtectedPasswordEntryPinging);
  if (password_entry_field_trial) {
    user_population->add_finch_active_groups(
        low_reputation_field_trial->trial_name() + "|" +
        low_reputation_field_trial->group_name());
  }
}

void PasswordProtectionService::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  if (stored_verdict_count_ <= 0)
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PasswordProtectionService::RemoveContentSettingsOnURLsDeleted,
                 GetWeakPtr(), all_history, deleted_rows));
}

void PasswordProtectionService::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observer_.RemoveAll();
}

void PasswordProtectionService::RemoveContentSettingsOnURLsDeleted(
    bool all_history,
    const history::URLRows& deleted_rows) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(content_settings_);

  if (all_history) {
    content_settings_->ClearSettingsForOneType(
        CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION);
    stored_verdict_count_ = 0;
    return;
  }

  // For now, if a URL is deleted from history, we simply remove all the
  // cached verdicts of the same origin. This is a pretty aggressive deletion.
  // We might revisit this logic later to decide if we want to only delete the
  // cached verdict whose cache expression matches this URL.
  for (const history::URLRow& row : deleted_rows) {
    if (!row.url().SchemeIsHTTPOrHTTPS())
      continue;
    GURL url_key = GetHostNameWithHTTPScheme(row.url());
    std::unique_ptr<base::DictionaryValue> verdict_dictionary =
        base::DictionaryValue::From(content_settings_->GetWebsiteSetting(
            url_key, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
            std::string(), nullptr));

    // Move on if we have no cached verdict for this deleted history row.
    if (!verdict_dictionary.get() || verdict_dictionary->empty())
      continue;

    int verdict_count = static_cast<int>(verdict_dictionary->size());
    stored_verdict_count_ = GetStoredVerdictCount() - verdict_count;
    content_settings_->ClearSettingsForOneTypeWithPredicate(
        CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, base::Time(),
        base::Bind(&OriginMatchPrimaryPattern, url_key));
  }
}

// static
bool PasswordProtectionService::ParseVerdictEntry(
    base::DictionaryValue* verdict_entry,
    int* out_verdict_received_time,
    LoginReputationClientResponse* out_verdict) {
  std::string serialized_verdict_proto;
  return verdict_entry && out_verdict &&
         verdict_entry->GetInteger(kCacheCreationTime,
                                   out_verdict_received_time) &&
         verdict_entry->GetString(kVerdictProto, &serialized_verdict_proto) &&
         base::Base64Decode(serialized_verdict_proto,
                            &serialized_verdict_proto) &&
         out_verdict->ParseFromString(serialized_verdict_proto);
}

bool PasswordProtectionService::PathVariantsMatchCacheExpression(
    const std::vector<std::string>& generated_paths,
    const std::string& cache_expression_path) {
  for (const auto& path : generated_paths) {
    if (cache_expression_path == path)
      return true;
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
  // TODO(jialiul): Change this to a DCHECk when SB server is ready.
  if (cache_expression.empty())
    return std::string("/");

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
  std::string serialized_proto(verdict->SerializeAsString());
  // Performs a base64 encoding on the serialized proto.
  base::Base64Encode(serialized_proto, &serialized_proto);
  result->SetString(kVerdictProto, serialized_proto);
  return result;
}

void PasswordProtectionService::RecordNoPingingReason(
    const base::Feature& feature,
    RequestOutcome reason) {
  DCHECK(feature.name == kProtectedPasswordEntryPinging.name ||
         feature.name == kPasswordFieldOnFocusPinging.name);

  bool is_password_entry_ping =
      feature.name == kProtectedPasswordEntryPinging.name;

  if (is_password_entry_ping) {
    UMA_HISTOGRAM_ENUMERATION(kPasswordEntryRequestOutcomeHistogramName, reason,
                              MAX_OUTCOME);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kPasswordOnFocusRequestOutcomeHistogramName,
                              reason, MAX_OUTCOME);
  }
}

}  // namespace safe_browsing
