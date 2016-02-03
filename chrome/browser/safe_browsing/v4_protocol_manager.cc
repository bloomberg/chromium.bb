// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/v4_protocol_manager.h"

#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/timer/timer.h"
#include "google_apis/google_api_keys.h"
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

// Enumerate parsing failures for histogramming purposes.  DO NOT CHANGE
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

  // A match in the response contained no metadata where metadata was
  // expected.
  NO_METADATA_ERROR = 4,

  // A match in the response contained a ThreatType that was inconsistent
  // with the other matches.
  INCONSISTENT_THREAT_TYPE_ERROR = 5,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  PARSE_GET_HASH_RESULT_MAX = 6
};

// Record parsing errors of a GetHash result.
void RecordParseGetHashResult(ParseResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.ParseV4HashResult", result_type,
                            PARSE_GET_HASH_RESULT_MAX);
}

}  // namespace

namespace safe_browsing {

const char kUmaV4ResponseMetricName[] =
    "SafeBrowsing.GetV4HashHttpResponseOrErrorCode";

// The URL prefix where browser fetches hashes from the server.
const char kSbV4UrlPrefix[] = "https://safebrowsing.googleapis.com/v4";

// The default SBProtocolManagerFactory.
class V4ProtocolManagerFactoryImpl : public V4ProtocolManagerFactory {
 public:
  V4ProtocolManagerFactoryImpl() {}
  ~V4ProtocolManagerFactoryImpl() override {}
  V4ProtocolManager* CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const SafeBrowsingProtocolConfig& config) override {
    return new V4ProtocolManager(request_context_getter, config);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(V4ProtocolManagerFactoryImpl);
};

// V4ProtocolManager implementation --------------------------------

// static
V4ProtocolManagerFactory* V4ProtocolManager::factory_ = NULL;

// static
V4ProtocolManager* V4ProtocolManager::Create(
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config) {
  if (!factory_)
    factory_ = new V4ProtocolManagerFactoryImpl();
  return factory_->CreateProtocolManager(request_context_getter, config);
}

// static
// Backoff interval is MIN(((2^(n-1))*15 minutes) * (RAND + 1), 24 hours) where
// n is the number of consecutive errors.
base::TimeDelta V4ProtocolManager::GetNextBackOffInterval(size_t* error_count,
                                                          size_t* multiplier) {
  DCHECK(multiplier && error_count);
  (*error_count)++;
  if (*error_count > 1 && *error_count < 9) {
    // With error count 9 and above we will hit the 24 hour max interval.
    // Cap the multiplier here to prevent integer overflow errors.
    *multiplier *= 2;
  }
  base::TimeDelta next =
      base::TimeDelta::FromMinutes(*multiplier * (1 + base::RandDouble()) * 15);

  base::TimeDelta day = base::TimeDelta::FromHours(24);

  if (next < day)
    return next;
  else
    return day;
}

void V4ProtocolManager::ResetGetHashErrors() {
  gethash_error_count_ = 0;
  gethash_back_off_mult_ = 1;
}

V4ProtocolManager::V4ProtocolManager(
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config)
    : gethash_error_count_(0),
      gethash_back_off_mult_(1),
      next_gethash_time_(Time::FromDoubleT(0)),
      version_(config.version),
      client_name_(config.client_name),
      request_context_getter_(request_context_getter),
      url_fetcher_id_(0) {
  if (version_.empty())
    version_ = SafeBrowsingProtocolManagerHelper::Version();
}

// static
void V4ProtocolManager::RecordGetHashResult(ResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.GetV4HashResult", result_type,
                            GET_HASH_RESULT_MAX);
}

void V4ProtocolManager::RecordHttpResponseOrErrorCode(
    const char* metric_name,
    const net::URLRequestStatus& status,
    int response_code) {
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      metric_name, status.is_success() ? response_code : status.error());
}

V4ProtocolManager::~V4ProtocolManager() {
  // Delete in-progress SafeBrowsing requests.
  STLDeleteContainerPairFirstPointers(hash_requests_.begin(),
                                      hash_requests_.end());
  hash_requests_.clear();
}

std::string V4ProtocolManager::GetHashRequest(
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

bool V4ProtocolManager::ParseHashResponse(
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
    next_gethash_time_ =
        Time::Now() + base::TimeDelta::FromSeconds(
                          response.minimum_wait_duration().seconds());
  }

  // We only expect one threat type per request, so we make sure
  // the threat types are consistent between matches.
  ThreatType expected_threat_type = THREAT_TYPE_UNSPECIFIED;

  // Loop over the threat matches and fill in full_hashes.
  for (const ThreatMatch& match : response.matches()) {
    // Make sure the platform and threat entry type match.
    if (!(match.has_threat_entry_type() &&
          match.threat_entry_type() == URL_EXPRESSION && match.has_threat())) {
      RecordParseGetHashResult(UNEXPECTED_THREAT_ENTRY_TYPE_ERROR);
      return false;
    }

    if (!match.has_threat_type()) {
      RecordParseGetHashResult(UNEXPECTED_THREAT_TYPE_ERROR);
      return false;
    }

    if (expected_threat_type == THREAT_TYPE_UNSPECIFIED) {
      expected_threat_type = match.threat_type();
    } else if (match.threat_type() != expected_threat_type) {
      RecordParseGetHashResult(INCONSISTENT_THREAT_TYPE_ERROR);
      return false;
    }

    // Fill in the full hash.
    SBFullHashResult result;
    result.hash = StringToSBFullHash(match.threat().hash());

    if (match.has_cache_duration()) {
      // Seconds resolution is good enough so we ignore the nanos field.
      result.cache_duration =
          base::TimeDelta::FromSeconds(match.cache_duration().seconds());
    }

    // Different threat types will handle the metadata differently.
    if (match.threat_type() == API_ABUSE) {
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

void V4ProtocolManager::GetFullHashes(
    const std::vector<SBPrefix>& prefixes,
    const std::vector<PlatformType>& platforms,
    ThreatType threat_type,
    FullHashCallback callback) {
  DCHECK(CalledOnValidThread());
  // We need to wait the minimum waiting duration, and if we are in backoff,
  // we need to check if we're past the next allowed time. If we are, we can
  // proceed with the request. If not, we are required to return empty results
  // (i.e. treat the page as safe).
  if (Time::Now() <= next_gethash_time_) {
    if (gethash_error_count_) {
      RecordGetHashResult(GET_HASH_BACKOFF_ERROR);
    } else {
      RecordGetHashResult(GET_HASH_MIN_WAIT_DURATION_ERROR);
    }
    std::vector<SBFullHashResult> full_hashes;
    callback.Run(full_hashes, base::TimeDelta());
    return;
  }

  std::string req_base64 = GetHashRequest(prefixes, platforms, threat_type);
  GURL gethash_url = GetHashUrl(req_base64);

  net::URLFetcher* fetcher =
      net::URLFetcher::Create(url_fetcher_id_++, gethash_url,
                              net::URLFetcher::GET, this)
          .release();
  hash_requests_[fetcher] = callback;

  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->Start();
}

void V4ProtocolManager::GetFullHashesWithApis(
    const std::vector<SBPrefix>& prefixes,
    FullHashCallback callback) {
  std::vector<PlatformType> platform = {CHROME_PLATFORM};
  GetFullHashes(prefixes, platform, API_ABUSE, callback);
}

// net::URLFetcherDelegate implementation ----------------------------------

// SafeBrowsing request responses are handled here.
void V4ProtocolManager::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());

  HashRequests::iterator it = hash_requests_.find(source);
  DCHECK(it != hash_requests_.end()) << "Request not found";

  // FindFullHashes response.
  // Reset the scoped pointer so the fetcher gets destroyed properly.
  scoped_ptr<const net::URLFetcher> fetcher(it->first);

  int response_code = source->GetResponseCode();
  net::URLRequestStatus status = source->GetStatus();
  RecordHttpResponseOrErrorCode(kUmaV4ResponseMetricName, status,
                                response_code);

  const FullHashCallback& callback = it->second;
  std::vector<SBFullHashResult> full_hashes;
  base::TimeDelta negative_cache_duration;
  if (status.is_success() && response_code == net::HTTP_OK) {
    RecordGetHashResult(GET_HASH_STATUS_200);
    ResetGetHashErrors();
    std::string data;
    source->GetResponseAsString(&data);
    if (!ParseHashResponse(data, &full_hashes, &negative_cache_duration)) {
      full_hashes.clear();
      RecordGetHashResult(GET_HASH_PARSE_ERROR);
    }
  } else {
    HandleGetHashError(Time::Now());

    DVLOG(1) << "SafeBrowsing GetEncodedFullHashes request for: "
             << source->GetURL() << " failed with error: " << status.error()
             << " and response code: " << response_code;

    if (status.status() == net::URLRequestStatus::FAILED) {
      RecordGetHashResult(GET_HASH_NETWORK_ERROR);
    } else {
      RecordGetHashResult(GET_HASH_HTTP_ERROR);
    }
  }

  // Invoke the callback with full_hashes, even if there was a parse error or
  // an error response code (in which case full_hashes will be empty). The
  // caller can't be blocked indefinitely.
  callback.Run(full_hashes, negative_cache_duration);

  hash_requests_.erase(it);
}

void V4ProtocolManager::HandleGetHashError(const Time& now) {
  DCHECK(CalledOnValidThread());
  base::TimeDelta next = GetNextBackOffInterval(&gethash_error_count_,
                                                &gethash_back_off_mult_);
  next_gethash_time_ = now + next;
}

// The API hash call uses the pver4 Safe Browsing server.
GURL V4ProtocolManager::GetHashUrl(const std::string& request_base64) const {
  std::string url = SafeBrowsingProtocolManagerHelper::ComposePver4Url(
      kSbV4UrlPrefix, "encodedFullHashes", request_base64, client_name_,
      version_);
  return GURL(url);
}

}  // namespace safe_browsing
