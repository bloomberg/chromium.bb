// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/protocol_manager.h"

#include <utility>

#include "base/base64.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/timer/timer.h"
#include "chrome/browser/safe_browsing/protocol_parser.h"
#include "chrome/common/env_vars.h"
#include "components/safe_browsing_db/util.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using base::Time;
using base::TimeDelta;

namespace {

// UpdateResult indicates what happened with the primary and/or backup update
// requests. The ordering of the values must stay the same for UMA consistency,
// and is also ordered in this way to match ProtocolManager::BackupUpdateReason.
enum UpdateResult {
  UPDATE_RESULT_FAIL,
  UPDATE_RESULT_SUCCESS,
  UPDATE_RESULT_BACKUP_CONNECT_FAIL,
  UPDATE_RESULT_BACKUP_CONNECT_SUCCESS,
  UPDATE_RESULT_BACKUP_HTTP_FAIL,
  UPDATE_RESULT_BACKUP_HTTP_SUCCESS,
  UPDATE_RESULT_BACKUP_NETWORK_FAIL,
  UPDATE_RESULT_BACKUP_NETWORK_SUCCESS,
  UPDATE_RESULT_MAX,
  UPDATE_RESULT_BACKUP_START = UPDATE_RESULT_BACKUP_CONNECT_FAIL,
};

void RecordUpdateResult(UpdateResult result) {
  DCHECK(result >= 0 && result < UPDATE_RESULT_MAX);
  UMA_HISTOGRAM_ENUMERATION("SB2.UpdateResult", result, UPDATE_RESULT_MAX);
}

const char kSBUpdateFrequencyFinchExperiment[] = "SafeBrowsingUpdateFrequency";
const char kSBUpdateFrequencyFinchParam[] = "NextUpdateIntervalInMinutes";

// This will be used for experimenting on a small subset of the population to
// better estimate the benefit of updating the safe browsing hashes more
// frequently.
base::TimeDelta GetNextUpdateIntervalFromFinch() {
  std::string num_str = variations::GetVariationParamValue(
      kSBUpdateFrequencyFinchExperiment, kSBUpdateFrequencyFinchParam);
  int finch_next_update_interval_minutes = 0;
  if (!base::StringToInt(num_str, &finch_next_update_interval_minutes)) {
    finch_next_update_interval_minutes = 0;  // Defaults to 0.
  }
  return base::TimeDelta::FromMinutes(finch_next_update_interval_minutes);
}

// Enumerate V4 parsing failures for histogramming purposes.  DO NOT CHANGE
// THE ORDERING OF THESE VALUES.
enum ParseResultType {
  // Error parsing the protocol buffer from a string.
  PARSE_FROM_STRING_ERROR = 0,

  // A match in the response had an unexpected THREAT_ENTRY_TYPE.
  UNEXPECTED_THREAT_ENTRY_TYPE_ERROR = 1,

  // A match in the response had an unexpected THREAT_TYPE.
  UNEXPECTED_THREAT_TYPE_ERROR = 2,

  // A match in the response had an unexpected PLATFORM_TYPE.
  UNEXPECTED_PLATFORM_TYPE_ERROR = 3,

  // A match in teh response contained no metadata where metadata was
  // expected.
  NO_METADATA_ERROR = 4,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  PARSE_GET_HASH_RESULT_MAX = 5
};

// Record parsing errors of a GetHash result.
void RecordParseGetHashResult(ParseResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.ParseV4HashResult", result_type,
                            PARSE_GET_HASH_RESULT_MAX);
}

}  // namespace

namespace safe_browsing {

// Minimum time, in seconds, from start up before we must issue an update query.
static const int kSbTimerStartIntervalSecMin = 60;

// Maximum time, in seconds, from start up before we must issue an update query.
static const int kSbTimerStartIntervalSecMax = 300;

// The maximum time, in seconds, to wait for a response to an update request.
static const int kSbMaxUpdateWaitSec = 30;

// Maximum back off multiplier.
static const size_t kSbMaxBackOff = 8;

const char kGetHashUmaResponseMetricName[] = "SB2.GetHashResponseOrErrorCode";
const char kGetChunkUmaResponseMetricName[] = "SB2.GetChunkResponseOrErrorCode";
const char kUmaV4ResponseMetricName[] =
    "SafeBrowsing.GetV4HashHttpResponseOrErrorCode";

// The V4 URL prefix where browser fetches hashes from the V4 server.
const char kSbV4UrlPrefix[] = "https://safebrowsing.googleapis.com/v4";

// The default SBProtocolManagerFactory.
class SBProtocolManagerFactoryImpl : public SBProtocolManagerFactory {
 public:
  SBProtocolManagerFactoryImpl() {}
  ~SBProtocolManagerFactoryImpl() override {}
  SafeBrowsingProtocolManager* CreateProtocolManager(
      SafeBrowsingProtocolManagerDelegate* delegate,
      net::URLRequestContextGetter* request_context_getter,
      const SafeBrowsingProtocolConfig& config) override {
    return new SafeBrowsingProtocolManager(delegate, request_context_getter,
                                           config);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SBProtocolManagerFactoryImpl);
};

// SafeBrowsingProtocolManager implementation ----------------------------------

// static
SBProtocolManagerFactory* SafeBrowsingProtocolManager::factory_ = NULL;

// static
SafeBrowsingProtocolManager* SafeBrowsingProtocolManager::Create(
    SafeBrowsingProtocolManagerDelegate* delegate,
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config) {
  // TODO(cbentzel): Remove ScopedTracker below once crbug.com/483689 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "483689 SafeBrowsingProtocolManager::Create"));
  if (!factory_)
    factory_ = new SBProtocolManagerFactoryImpl();
  return factory_->CreateProtocolManager(delegate, request_context_getter,
                                         config);
}

// static
// Backoff interval is MIN(((2^(n-1))*15 minutes) * (RAND + 1), 24 hours) where
// n is the number of consecutive errors.
base::TimeDelta SafeBrowsingProtocolManager::GetNextV4BackOffInterval(
    size_t* error_count,
    size_t* multiplier) {
  DCHECK(multiplier && error_count);
  (*error_count)++;
  if (*error_count > 1 && *error_count < 9) {
    // With error count 9 and above we will hit the 24 hour max interval.
    // Cap the multiplier here to prevent integer overflow errors.
    *multiplier *= 2;
  }
  base::TimeDelta next = base::TimeDelta::FromMinutes(
      *multiplier * (1 + base::RandDouble()) * 15);

  base::TimeDelta day = base::TimeDelta::FromHours(24);

  if (next < day)
    return next;
  else
    return day;
}

void SafeBrowsingProtocolManager::ResetGetHashV4Errors() {
  gethash_v4_error_count_ = 0;
  gethash_v4_back_off_mult_ = 1;
}

SafeBrowsingProtocolManager::SafeBrowsingProtocolManager(
    SafeBrowsingProtocolManagerDelegate* delegate,
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config)
    : delegate_(delegate),
      request_type_(NO_REQUEST),
      update_error_count_(0),
      gethash_error_count_(0),
      gethash_v4_error_count_(0),
      update_back_off_mult_(1),
      gethash_back_off_mult_(1),
      gethash_v4_back_off_mult_(1),
      next_update_interval_(base::TimeDelta::FromSeconds(
          base::RandInt(kSbTimerStartIntervalSecMin,
                        kSbTimerStartIntervalSecMax))),
      chunk_pending_to_write_(false),
      next_gethash_v4_time_(Time::FromDoubleT(0)),
      version_(config.version),
      update_size_(0),
      client_name_(config.client_name),
      request_context_getter_(request_context_getter),
      url_prefix_(config.url_prefix),
      backup_update_reason_(BACKUP_UPDATE_REASON_MAX),
      disable_auto_update_(config.disable_auto_update),
      url_fetcher_id_(0) {
  DCHECK(!url_prefix_.empty());

  backup_url_prefixes_[BACKUP_UPDATE_REASON_CONNECT] =
      config.backup_connect_error_url_prefix;
  backup_url_prefixes_[BACKUP_UPDATE_REASON_HTTP] =
      config.backup_http_error_url_prefix;
  backup_url_prefixes_[BACKUP_UPDATE_REASON_NETWORK] =
      config.backup_network_error_url_prefix;

  // Set the backoff multiplier fuzz to a random value between 0 and 1.
  back_off_fuzz_ = static_cast<float>(base::RandDouble());
  if (version_.empty())
    version_ = SafeBrowsingProtocolManagerHelper::Version();
}

// static
void SafeBrowsingProtocolManager::RecordGetHashResult(bool is_download,
                                                      ResultType result_type) {
  if (is_download) {
    UMA_HISTOGRAM_ENUMERATION("SB2.GetHashResultDownload", result_type,
                              GET_HASH_RESULT_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION("SB2.GetHashResult", result_type,
                              GET_HASH_RESULT_MAX);
  }
}

// static
void SafeBrowsingProtocolManager::RecordGetV4HashResult(
    ResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.GetV4HashResult", result_type,
                            GET_HASH_RESULT_MAX);
}

void SafeBrowsingProtocolManager::RecordHttpResponseOrErrorCode(
    const char* metric_name,
    const net::URLRequestStatus& status,
    int response_code) {
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      metric_name, status.is_success() ? response_code : status.error());
}

bool SafeBrowsingProtocolManager::IsUpdateScheduled() const {
  return update_timer_.IsRunning();
}

SafeBrowsingProtocolManager::~SafeBrowsingProtocolManager() {
  // Delete in-progress SafeBrowsing requests.
  STLDeleteContainerPairFirstPointers(hash_requests_.begin(),
                                      hash_requests_.end());
  hash_requests_.clear();

  STLDeleteContainerPairFirstPointers(v4_hash_requests_.begin(),
                                      v4_hash_requests_.end());
  v4_hash_requests_.clear();
}

// We can only have one update or chunk request outstanding, but there may be
// multiple GetHash requests pending since we don't want to serialize them and
// slow down the user.
void SafeBrowsingProtocolManager::GetFullHash(
    const std::vector<SBPrefix>& prefixes,
    FullHashCallback callback,
    bool is_download,
    bool is_extended_reporting) {
  DCHECK(CalledOnValidThread());
  // If we are in GetHash backoff, we need to check if we're past the next
  // allowed time. If we are, we can proceed with the request. If not, we are
  // required to return empty results (i.e. treat the page as safe).
  if (gethash_error_count_ && Time::Now() <= next_gethash_time_) {
    RecordGetHashResult(is_download, GET_HASH_BACKOFF_ERROR);
    std::vector<SBFullHashResult> full_hashes;
    callback.Run(full_hashes, base::TimeDelta());
    return;
  }
  GURL gethash_url = GetHashUrl(is_extended_reporting);
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(url_fetcher_id_++, gethash_url,
                              net::URLFetcher::POST, this)
          .release();
  hash_requests_[fetcher] = FullHashDetails(callback, is_download);

  const std::string get_hash = FormatGetHash(prefixes);

  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->SetUploadData("text/plain", get_hash);
  fetcher->Start();
}

std::string SafeBrowsingProtocolManager::GetV4HashRequest(
    const std::vector<SBPrefix>& prefixes,
    const std::vector<PlatformType>& platforms,
    ThreatType threat_type) {
  // Build the request. Client info and client states are not added to the
  // request protocol buffer. Client info is passed as params in the url.
  FindFullHashesRequest req;
  ThreatInfo* info = req.mutable_threat_info();
  info->add_threat_types(threat_type);
  info->add_threat_entry_types(URL_EXPRESSION);
  for (const PlatformType p : platforms) {
    info->add_platform_types(p);
  }
  for (const SBPrefix& prefix : prefixes) {
    std::string hash(reinterpret_cast<const char*>(&prefix), sizeof(SBPrefix));
    info->add_threat_entries()->set_hash(hash);
  }

  // Serialize and Base64 encode.
  std::string req_data, req_base64;
  req.SerializeToString(&req_data);
  base::Base64Encode(req_data, &req_base64);

  return req_base64;
}

bool SafeBrowsingProtocolManager::ParseV4HashResponse(
    const std::string& data,
    std::vector<SBFullHashResult>* full_hashes,
    base::TimeDelta* negative_cache_duration) {
  FindFullHashesResponse response;

  if (!response.ParseFromString(data)) {
    RecordParseGetHashResult(PARSE_FROM_STRING_ERROR);
    return false;
  }

  if (response.has_negative_cache_duration()) {
    // Seconds resolution is good enough so we ignore the nanos field.
    *negative_cache_duration = base::TimeDelta::FromSeconds(
        response.negative_cache_duration().seconds());
  }

  if (response.has_minimum_wait_duration()) {
    // Seconds resolution is good enough so we ignore the nanos field.
    next_gethash_v4_time_ = Time::Now() + base::TimeDelta::FromSeconds(
        response.minimum_wait_duration().seconds());
  }

  // We only expect one threat type per request, so we make sure
  // the threat types are consistent between matches.
  ThreatType expected_threat_type = THREAT_TYPE_UNSPECIFIED;

  // Loop over the threat matches and fill in full_hashes.
  for (const ThreatMatch& match : response.matches()) {
    // Make sure the platform and threat entry type match.
    if (!(match.has_threat_entry_type() &&
          match.threat_entry_type() == URL_EXPRESSION &&
          match.has_threat())) {
      RecordParseGetHashResult(UNEXPECTED_THREAT_ENTRY_TYPE_ERROR);
      return false;
    }

    if (!match.has_threat_type()) {
      // TODO(kcarattini): Add UMA.
      return false;
    }

    if (expected_threat_type == THREAT_TYPE_UNSPECIFIED) {
      expected_threat_type = match.threat_type();
    } else if (match.threat_type() != expected_threat_type) {
      // TODO(kcarattini): Add UMA.
      return false;
    }

    // Fill in the full hash.
    SBFullHashResult result;
    result.hash = StringToSBFullHash(match.threat().hash());

    if (match.has_cache_duration()) {
      // Seconds resolution is good enough so we ignore the nanos field.
      result.cache_duration = base::TimeDelta::FromSeconds(
          match.cache_duration().seconds());
    }

    // Different threat types will handle the metadata differently.
    if (match.has_threat_type() && match.threat_type() == API_ABUSE) {
      if (match.has_platform_type() &&
          match.platform_type() == CHROME_PLATFORM) {
        if (match.has_threat_entry_metadata()) {
          // For API Abuse, store a csv of the returned permissions.
          for (const ThreatEntryMetadata::MetadataEntry& m :
                   match.threat_entry_metadata().entries()) {
            if (m.key() == "permission") {
              result.metadata += m.value() + ",";
            }
          }
        } else {
          RecordParseGetHashResult(NO_METADATA_ERROR);
          return false;
        }
      } else {
        RecordParseGetHashResult(UNEXPECTED_PLATFORM_TYPE_ERROR);
        return false;
      }
    } else {
      RecordParseGetHashResult(UNEXPECTED_THREAT_TYPE_ERROR);
      return false;
    }

    full_hashes->push_back(result);
  }
  return true;
}

void SafeBrowsingProtocolManager::GetV4FullHashes(
    const std::vector<SBPrefix>& prefixes,
    const std::vector<PlatformType>& platforms,
    ThreatType threat_type,
    FullHashCallback callback) {
  DCHECK(CalledOnValidThread());
  // We need to wait the minimum waiting duration, and if we are in backoff,
  // we need to check if we're past the next allowed time. If we are, we can
  // proceed with the request. If not, we are required to return empty results
  // (i.e. treat the page as safe).
  if (Time::Now() <= next_gethash_v4_time_) {
    if (gethash_v4_error_count_) {
      RecordGetV4HashResult(GET_HASH_BACKOFF_ERROR);
    } else {
      RecordGetV4HashResult(GET_HASH_MIN_WAIT_DURATION_ERROR);
    }
    std::vector<SBFullHashResult> full_hashes;
    callback.Run(full_hashes, base::TimeDelta());
    return;
  }

  std::string req_base64 = GetV4HashRequest(prefixes, platforms, threat_type);
  GURL gethash_url = GetV4HashUrl(req_base64);

  net::URLFetcher* fetcher =
      net::URLFetcher::Create(url_fetcher_id_++, gethash_url,
                              net::URLFetcher::GET, this)
          .release();
  v4_hash_requests_[fetcher] = FullHashDetails(callback,
                                               false  /* is_download */);

  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->Start();
}

void SafeBrowsingProtocolManager::GetFullHashesWithApis(
    const std::vector<SBPrefix>& prefixes,
    FullHashCallback callback) {
  std::vector<PlatformType> platform = {CHROME_PLATFORM};
  GetV4FullHashes(prefixes, platform, API_ABUSE, callback);
}

void SafeBrowsingProtocolManager::GetNextUpdate() {
  DCHECK(CalledOnValidThread());
  if (request_.get() || request_type_ != NO_REQUEST)
    return;

  IssueUpdateRequest();
}

// net::URLFetcherDelegate implementation ----------------------------------

// All SafeBrowsing request responses are handled here.
// TODO(paulg): Clarify with the SafeBrowsing team whether a failed parse of a
//              chunk should retry the download and parse of that chunk (and
//              what back off / how many times to try), and if that effects the
//              update back off. For now, a failed parse of the chunk means we
//              drop it. This isn't so bad because the next UPDATE_REQUEST we
//              do will report all the chunks we have. If that chunk is still
//              required, the SafeBrowsing servers will tell us to get it again.
void SafeBrowsingProtocolManager::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  scoped_ptr<const net::URLFetcher> fetcher;

  HashRequests::iterator it = hash_requests_.find(source);
  HashRequests::iterator v4_it = v4_hash_requests_.find(source);
  int response_code = source->GetResponseCode();
  net::URLRequestStatus status = source->GetStatus();

  if (it != hash_requests_.end()) {
    // GetHash response.
    // Reset the scoped pointer so the fetcher gets destroyed properly.
    fetcher.reset(it->first);
    RecordHttpResponseOrErrorCode(kGetHashUmaResponseMetricName, status,
                                  response_code);
    const FullHashDetails& details = it->second;
    std::vector<SBFullHashResult> full_hashes;
    base::TimeDelta cache_lifetime;
    if (status.is_success() && (response_code == net::HTTP_OK ||
                                response_code == net::HTTP_NO_CONTENT)) {
      // For tracking our GetHash false positive (net::HTTP_NO_CONTENT) rate,
      // compared to real (net::HTTP_OK) responses.
      if (response_code == net::HTTP_OK)
        RecordGetHashResult(details.is_download, GET_HASH_STATUS_200);
      else
        RecordGetHashResult(details.is_download, GET_HASH_STATUS_204);

      gethash_error_count_ = 0;
      gethash_back_off_mult_ = 1;
      std::string data;
      source->GetResponseAsString(&data);
      if (!ParseGetHash(data.data(), data.length(), &cache_lifetime,
                        &full_hashes)) {
        full_hashes.clear();
        RecordGetHashResult(details.is_download, GET_HASH_PARSE_ERROR);
        // TODO(cbentzel): Should cache_lifetime be set to 0 here? (See
        // http://crbug.com/360232.)
      }
    } else {
      HandleGetHashError(Time::Now());
      if (status.status() == net::URLRequestStatus::FAILED) {
        RecordGetHashResult(details.is_download, GET_HASH_NETWORK_ERROR);
        DVLOG(1) << "SafeBrowsing GetHash request for: " << source->GetURL()
                 << " failed with error: " << status.error();
      } else {
        RecordGetHashResult(details.is_download, GET_HASH_HTTP_ERROR);
        DVLOG(1) << "SafeBrowsing GetHash request for: " << source->GetURL()
                 << " failed with error: " << response_code;
      }
    }

    // Invoke the callback with full_hashes, even if there was a parse error or
    // an error response code (in which case full_hashes will be empty). The
    // caller can't be blocked indefinitely.
    details.callback.Run(full_hashes, cache_lifetime);

    hash_requests_.erase(it);
  } else if (v4_it != v4_hash_requests_.end()) {
    // V4 FindFullHashes response.
    // TODO(kcarattini): Consider pulling all the V4 handling out into a
    // separate V4ProtocolManager class.

    // Reset the scoped pointer so the fetcher gets destroyed properly.
    fetcher.reset(v4_it->first);
    RecordHttpResponseOrErrorCode(kUmaV4ResponseMetricName, status,
                                  response_code);
    const FullHashDetails& details = v4_it->second;
    std::vector<SBFullHashResult> full_hashes;
    base::TimeDelta negative_cache_duration;
    if (status.is_success() && response_code == net::HTTP_OK) {
      RecordGetV4HashResult(GET_HASH_STATUS_200);
      ResetGetHashV4Errors();
      std::string data;
      source->GetResponseAsString(&data);
      if (!ParseV4HashResponse(data, &full_hashes, &negative_cache_duration)) {
        full_hashes.clear();
        RecordGetV4HashResult(GET_HASH_PARSE_ERROR);
      }
    } else {
      HandleGetHashV4Error(Time::Now());

      DVLOG(1) << "SafeBrowsing GetEncodedFullHashes request for: " <<
          source->GetURL() << " failed with error: " << status.error() <<
          " and response code: " << response_code;

      if (status.status() == net::URLRequestStatus::FAILED) {
        RecordGetV4HashResult(GET_HASH_NETWORK_ERROR);
      } else {
        RecordGetV4HashResult(GET_HASH_HTTP_ERROR);
      }
    }

    // Invoke the callback with full_hashes, even if there was a parse error or
    // an error response code (in which case full_hashes will be empty). The
    // caller can't be blocked indefinitely.
    details.callback.Run(full_hashes, negative_cache_duration);

    v4_hash_requests_.erase(v4_it);
  } else {
    // Update or chunk response.
    RecordHttpResponseOrErrorCode(kGetChunkUmaResponseMetricName, status,
                                  response_code);
    fetcher.reset(request_.release());

    if (request_type_ == UPDATE_REQUEST ||
        request_type_ == BACKUP_UPDATE_REQUEST) {
      if (!fetcher.get()) {
        // We've timed out waiting for an update response, so we've cancelled
        // the update request and scheduled a new one. Ignore this response.
        return;
      }

      // Cancel the update response timeout now that we have the response.
      timeout_timer_.Stop();
    }

    if (status.is_success() && response_code == net::HTTP_OK) {
      // We have data from the SafeBrowsing service.
      std::string data;
      source->GetResponseAsString(&data);

      // TODO(shess): Cleanup the flow of this code so that |parsed_ok| can be
      // removed or omitted.
      const bool parsed_ok =
          HandleServiceResponse(source->GetURL(), data.data(), data.length());
      if (!parsed_ok) {
        DVLOG(1) << "SafeBrowsing request for: " << source->GetURL()
                 << " failed parse.";
        chunk_request_urls_.clear();
        if (request_type_ == UPDATE_REQUEST &&
            IssueBackupUpdateRequest(BACKUP_UPDATE_REASON_HTTP)) {
          return;
        }
        UpdateFinished(false);
      }

      switch (request_type_) {
        case CHUNK_REQUEST:
          if (parsed_ok) {
            chunk_request_urls_.pop_front();
            if (chunk_request_urls_.empty() && !chunk_pending_to_write_)
              UpdateFinished(true);
          }
          break;
        case UPDATE_REQUEST:
        case BACKUP_UPDATE_REQUEST:
          if (chunk_request_urls_.empty() && parsed_ok) {
            // We are up to date since the servers gave us nothing new, so we
            // are done with this update cycle.
            UpdateFinished(true);
          }
          break;
        case NO_REQUEST:
          // This can happen if HandleServiceResponse fails above.
          break;
        default:
          NOTREACHED();
          break;
      }
    } else {
      if (status.status() == net::URLRequestStatus::FAILED) {
        DVLOG(1) << "SafeBrowsing request for: " << source->GetURL()
                 << " failed with error: " << status.error();
      } else {
        DVLOG(1) << "SafeBrowsing request for: " << source->GetURL()
                 << " failed with error: " << response_code;
      }
      if (request_type_ == CHUNK_REQUEST) {
        // The SafeBrowsing service error, or very bad response code: back off.
        chunk_request_urls_.clear();
      } else if (request_type_ == UPDATE_REQUEST) {
        BackupUpdateReason backup_update_reason = BACKUP_UPDATE_REASON_MAX;
        if (status.is_success()) {
          backup_update_reason = BACKUP_UPDATE_REASON_HTTP;
        } else {
          switch (status.error()) {
            case net::ERR_INTERNET_DISCONNECTED:
            case net::ERR_NETWORK_CHANGED:
              backup_update_reason = BACKUP_UPDATE_REASON_NETWORK;
              break;
            default:
              backup_update_reason = BACKUP_UPDATE_REASON_CONNECT;
              break;
          }
        }
        if (backup_update_reason != BACKUP_UPDATE_REASON_MAX &&
            IssueBackupUpdateRequest(backup_update_reason)) {
          return;
        }
      }
      UpdateFinished(false);
    }

    // Get the next chunk if available.
    IssueChunkRequest();
  }
}

bool SafeBrowsingProtocolManager::HandleServiceResponse(const GURL& url,
                                                        const char* data,
                                                        size_t length) {
  DCHECK(CalledOnValidThread());

  switch (request_type_) {
    case UPDATE_REQUEST:
    case BACKUP_UPDATE_REQUEST: {
      size_t next_update_sec = 0;
      bool reset = false;
      scoped_ptr<std::vector<SBChunkDelete>> chunk_deletes(
          new std::vector<SBChunkDelete>);
      std::vector<ChunkUrl> chunk_urls;
      if (!ParseUpdate(data, length, &next_update_sec, &reset,
                       chunk_deletes.get(), &chunk_urls)) {
        return false;
      }

      // New time for the next update.
      base::TimeDelta finch_next_update_interval =
          GetNextUpdateIntervalFromFinch();
      if (finch_next_update_interval > base::TimeDelta()) {
        next_update_interval_ = finch_next_update_interval;
      } else {
        base::TimeDelta next_update_interval =
            base::TimeDelta::FromSeconds(next_update_sec);
        if (next_update_interval > base::TimeDelta()) {
          next_update_interval_ = next_update_interval;
        }
      }
      last_update_ = Time::Now();

      // New chunks to download.
      if (!chunk_urls.empty()) {
        UMA_HISTOGRAM_COUNTS("SB2.UpdateUrls", chunk_urls.size());
        for (size_t i = 0; i < chunk_urls.size(); ++i)
          chunk_request_urls_.push_back(chunk_urls[i]);
      }

      // Handle the case were the SafeBrowsing service tells us to dump our
      // database.
      if (reset) {
        delegate_->ResetDatabase();
        return true;
      }

      // Chunks to delete from our storage.
      if (!chunk_deletes->empty())
        delegate_->DeleteChunks(std::move(chunk_deletes));

      break;
    }
    case CHUNK_REQUEST: {
      UMA_HISTOGRAM_TIMES("SB2.ChunkRequest",
                          base::Time::Now() - chunk_request_start_);

      const ChunkUrl chunk_url = chunk_request_urls_.front();
      scoped_ptr<std::vector<scoped_ptr<SBChunkData>>> chunks(
          new std::vector<scoped_ptr<SBChunkData>>);
      UMA_HISTOGRAM_COUNTS("SB2.ChunkSize", length);
      update_size_ += length;
      if (!ParseChunk(data, length, chunks.get()))
        return false;

      // Chunks to add to storage.  Pass ownership of |chunks|.
      if (!chunks->empty()) {
        chunk_pending_to_write_ = true;
        delegate_->AddChunks(
            chunk_url.list_name, std::move(chunks),
            base::Bind(&SafeBrowsingProtocolManager::OnAddChunksComplete,
                       base::Unretained(this)));
      }

      break;
    }

    default:
      return false;
  }

  return true;
}

void SafeBrowsingProtocolManager::Initialize() {
  DCHECK(CalledOnValidThread());
  // TODO(cbentzel): Remove ScopedTracker below once crbug.com/483689 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "483689 SafeBrowsingProtocolManager::Initialize"));
  // Don't want to hit the safe browsing servers on build/chrome bots.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar(env_vars::kHeadless))
    return;
  ScheduleNextUpdate(false /* no back off */);
}

void SafeBrowsingProtocolManager::ScheduleNextUpdate(bool back_off) {
  DCHECK(CalledOnValidThread());
  if (disable_auto_update_) {
    // Unschedule any current timer.
    update_timer_.Stop();
    return;
  }
  // Reschedule with the new update.
  base::TimeDelta next_update_interval = GetNextUpdateInterval(back_off);
  ForceScheduleNextUpdate(next_update_interval);
}

void SafeBrowsingProtocolManager::ForceScheduleNextUpdate(
    base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  DCHECK(interval >= base::TimeDelta());
  // Unschedule any current timer.
  update_timer_.Stop();
  update_timer_.Start(FROM_HERE, interval, this,
                      &SafeBrowsingProtocolManager::GetNextUpdate);
}

// According to section 5 of the SafeBrowsing protocol specification, we must
// back off after a certain number of errors. We only change |next_update_sec_|
// when we receive a response from the SafeBrowsing service.
base::TimeDelta SafeBrowsingProtocolManager::GetNextUpdateInterval(
    bool back_off) {
  DCHECK(CalledOnValidThread());
  DCHECK(next_update_interval_ > base::TimeDelta());
  base::TimeDelta next = next_update_interval_;
  if (back_off) {
    next = GetNextBackOffInterval(&update_error_count_, &update_back_off_mult_);
  } else {
    // Successful response means error reset.
    update_error_count_ = 0;
    update_back_off_mult_ = 1;
  }
  return next;
}

base::TimeDelta SafeBrowsingProtocolManager::GetNextBackOffInterval(
    size_t* error_count,
    size_t* multiplier) const {
  DCHECK(CalledOnValidThread());
  DCHECK(multiplier && error_count);
  (*error_count)++;
  if (*error_count > 1 && *error_count < 6) {
    base::TimeDelta next =
        base::TimeDelta::FromMinutes(*multiplier * (1 + back_off_fuzz_) * 30);
    *multiplier *= 2;
    if (*multiplier > kSbMaxBackOff)
      *multiplier = kSbMaxBackOff;
    return next;
  }
  if (*error_count >= 6)
    return base::TimeDelta::FromHours(8);
  return base::TimeDelta::FromMinutes(1);
}

// This request requires getting a list of all the chunks for each list from the
// database asynchronously. The request will be issued when we're called back in
// OnGetChunksComplete.
// TODO(paulg): We should get this at start up and maintain a ChunkRange cache
//              to avoid hitting the database with each update request. On the
//              otherhand, this request will only occur ~20-30 minutes so there
//              isn't that much overhead. Measure!
void SafeBrowsingProtocolManager::IssueUpdateRequest() {
  DCHECK(CalledOnValidThread());
  request_type_ = UPDATE_REQUEST;
  delegate_->UpdateStarted();
  delegate_->GetChunks(
      base::Bind(&SafeBrowsingProtocolManager::OnGetChunksComplete,
                 base::Unretained(this)));
}

// The backup request can run immediately since the chunks have already been
// retrieved from the DB.
bool SafeBrowsingProtocolManager::IssueBackupUpdateRequest(
    BackupUpdateReason backup_update_reason) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(request_type_, UPDATE_REQUEST);
  DCHECK(backup_update_reason >= 0 &&
         backup_update_reason < BACKUP_UPDATE_REASON_MAX);
  if (backup_url_prefixes_[backup_update_reason].empty())
    return false;
  request_type_ = BACKUP_UPDATE_REQUEST;
  backup_update_reason_ = backup_update_reason;

  GURL backup_update_url = BackupUpdateUrl(backup_update_reason);
  request_ = net::URLFetcher::Create(url_fetcher_id_++, backup_update_url,
                                     net::URLFetcher::POST, this);
  request_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  request_->SetRequestContext(request_context_getter_.get());
  request_->SetUploadData("text/plain", update_list_data_);
  request_->Start();

  // Begin the update request timeout.
  timeout_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(kSbMaxUpdateWaitSec),
                       this,
                       &SafeBrowsingProtocolManager::UpdateResponseTimeout);

  return true;
}

void SafeBrowsingProtocolManager::IssueChunkRequest() {
  DCHECK(CalledOnValidThread());
  // We are only allowed to have one request outstanding at any time.  Also,
  // don't get the next url until the previous one has been written to disk so
  // that we don't use too much memory.
  if (request_.get() || chunk_request_urls_.empty() || chunk_pending_to_write_)
    return;

  ChunkUrl next_chunk = chunk_request_urls_.front();
  DCHECK(!next_chunk.url.empty());
  GURL chunk_url = NextChunkUrl(next_chunk.url);
  request_type_ = CHUNK_REQUEST;
  request_ = net::URLFetcher::Create(url_fetcher_id_++, chunk_url,
                                     net::URLFetcher::GET, this);
  request_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  request_->SetRequestContext(request_context_getter_.get());
  chunk_request_start_ = base::Time::Now();
  request_->Start();
}

void SafeBrowsingProtocolManager::OnGetChunksComplete(
    const std::vector<SBListChunkRanges>& lists,
    bool database_error,
    bool is_extended_reporting) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(request_type_, UPDATE_REQUEST);
  DCHECK(update_list_data_.empty());
  if (database_error) {
    // The update was not successful, but don't back off.
    UpdateFinished(false, false);
    return;
  }

  // Format our stored chunks:
  bool found_malware = false;
  bool found_phishing = false;
  for (size_t i = 0; i < lists.size(); ++i) {
    update_list_data_.append(FormatList(lists[i]));
    if (lists[i].name == kPhishingList)
      found_phishing = true;

    if (lists[i].name == kMalwareList)
      found_malware = true;
  }

  // If we have an empty database, let the server know we want data for these
  // lists.
  // TODO(shess): These cases never happen because the database fills in the
  // lists in GetChunks().  Refactor the unit tests so that this code can be
  // removed.
  if (!found_phishing) {
    update_list_data_.append(FormatList(SBListChunkRanges(kPhishingList)));
  }
  if (!found_malware) {
    update_list_data_.append(FormatList(SBListChunkRanges(kMalwareList)));
  }

  // Large requests are (probably) a sign of database corruption.
  // Record stats to inform decisions about whether to automate
  // deletion of such databases.  http://crbug.com/120219
  UMA_HISTOGRAM_COUNTS("SB2.UpdateRequestSize", update_list_data_.size());

  GURL update_url = UpdateUrl(is_extended_reporting);
  request_ = net::URLFetcher::Create(url_fetcher_id_++, update_url,
                                     net::URLFetcher::POST, this);
  request_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  request_->SetRequestContext(request_context_getter_.get());
  request_->SetUploadData("text/plain", update_list_data_);
  request_->Start();

  // Begin the update request timeout.
  timeout_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(kSbMaxUpdateWaitSec),
                       this,
                       &SafeBrowsingProtocolManager::UpdateResponseTimeout);
}

// If we haven't heard back from the server with an update response, this method
// will run. Close the current update session and schedule another update.
void SafeBrowsingProtocolManager::UpdateResponseTimeout() {
  DCHECK(CalledOnValidThread());
  DCHECK(request_type_ == UPDATE_REQUEST ||
         request_type_ == BACKUP_UPDATE_REQUEST);
  request_.reset();
  if (request_type_ == UPDATE_REQUEST &&
      IssueBackupUpdateRequest(BACKUP_UPDATE_REASON_CONNECT)) {
    return;
  }
  UpdateFinished(false);
}

void SafeBrowsingProtocolManager::OnAddChunksComplete() {
  DCHECK(CalledOnValidThread());
  chunk_pending_to_write_ = false;

  if (chunk_request_urls_.empty()) {
    UMA_HISTOGRAM_LONG_TIMES("SB2.Update", Time::Now() - last_update_);
    UpdateFinished(true);
  } else {
    IssueChunkRequest();
  }
}

void SafeBrowsingProtocolManager::HandleGetHashError(const Time& now) {
  DCHECK(CalledOnValidThread());
  base::TimeDelta next =
      GetNextBackOffInterval(&gethash_error_count_, &gethash_back_off_mult_);
  next_gethash_time_ = now + next;
}

void SafeBrowsingProtocolManager::HandleGetHashV4Error(const Time& now) {
  DCHECK(CalledOnValidThread());
  base::TimeDelta next = GetNextV4BackOffInterval(
      &gethash_v4_error_count_, &gethash_v4_back_off_mult_);
  next_gethash_v4_time_ = now + next;
}

void SafeBrowsingProtocolManager::UpdateFinished(bool success) {
  UpdateFinished(success, !success);
}

void SafeBrowsingProtocolManager::UpdateFinished(bool success, bool back_off) {
  DCHECK(CalledOnValidThread());
  UMA_HISTOGRAM_COUNTS("SB2.UpdateSize", update_size_);
  update_size_ = 0;
  bool update_success = success || request_type_ == CHUNK_REQUEST;
  if (backup_update_reason_ == BACKUP_UPDATE_REASON_MAX) {
    RecordUpdateResult(update_success ? UPDATE_RESULT_SUCCESS
                                      : UPDATE_RESULT_FAIL);
  } else {
    UpdateResult update_result = static_cast<UpdateResult>(
        UPDATE_RESULT_BACKUP_START +
        (static_cast<int>(backup_update_reason_) * 2) + update_success);
    RecordUpdateResult(update_result);
  }
  backup_update_reason_ = BACKUP_UPDATE_REASON_MAX;
  request_type_ = NO_REQUEST;
  update_list_data_.clear();
  delegate_->UpdateFinished(success);
  ScheduleNextUpdate(back_off);
}

GURL SafeBrowsingProtocolManager::UpdateUrl(bool is_extended_reporting) const {
  std::string url = SafeBrowsingProtocolManagerHelper::ComposeUrl(
      url_prefix_, "downloads", client_name_, version_, additional_query_,
      is_extended_reporting);
  return GURL(url);
}

GURL SafeBrowsingProtocolManager::BackupUpdateUrl(
    BackupUpdateReason backup_update_reason) const {
  DCHECK(backup_update_reason >= 0 &&
         backup_update_reason < BACKUP_UPDATE_REASON_MAX);
  DCHECK(!backup_url_prefixes_[backup_update_reason].empty());
  std::string url = SafeBrowsingProtocolManagerHelper::ComposeUrl(
      backup_url_prefixes_[backup_update_reason], "downloads", client_name_,
      version_, additional_query_);
  return GURL(url);
}

GURL SafeBrowsingProtocolManager::GetHashUrl(bool is_extended_reporting) const {
  std::string url = SafeBrowsingProtocolManagerHelper::ComposeUrl(
      url_prefix_, "gethash", client_name_, version_, additional_query_,
      is_extended_reporting);
  return GURL(url);
}

// The API hash call uses the pver4 Safe Browsing server.
GURL SafeBrowsingProtocolManager::GetV4HashUrl(
    const std::string& request_base64) const {
  std::string url = SafeBrowsingProtocolManagerHelper::ComposePver4Url(
      kSbV4UrlPrefix, "encodedFullHashes",
      request_base64, client_name_, version_);
  return GURL(url);
}

GURL SafeBrowsingProtocolManager::NextChunkUrl(const std::string& url) const {
  DCHECK(CalledOnValidThread());
  std::string next_url;
  if (!base::StartsWith(url, "http://", base::CompareCase::INSENSITIVE_ASCII) &&
      !base::StartsWith(url, "https://",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    // Use https if we updated via https, otherwise http (useful for testing).
    if (base::StartsWith(url_prefix_, "https://",
                         base::CompareCase::INSENSITIVE_ASCII))
      next_url.append("https://");
    else
      next_url.append("http://");
    next_url.append(url);
  } else {
    next_url = url;
  }
  if (!additional_query_.empty()) {
    if (next_url.find("?") != std::string::npos) {
      next_url.append("&");
    } else {
      next_url.append("?");
    }
    next_url.append(additional_query_);
  }
  return GURL(next_url);
}

SafeBrowsingProtocolManager::FullHashDetails::FullHashDetails()
    : callback(), is_download(false) {}

SafeBrowsingProtocolManager::FullHashDetails::FullHashDetails(
    FullHashCallback callback,
    bool is_download)
    : callback(callback), is_download(is_download) {}

SafeBrowsingProtocolManager::FullHashDetails::~FullHashDetails() {}

SafeBrowsingProtocolManagerDelegate::~SafeBrowsingProtocolManagerDelegate() {}

}  // namespace safe_browsing
