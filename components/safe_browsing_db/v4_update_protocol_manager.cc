// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_update_protocol_manager.h"

#include <utility>

#include "base/base64url.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/timer/timer.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

using base::Time;
using base::TimeDelta;

namespace {

// Enumerate parsing failures for histogramming purposes.  DO NOT CHANGE
// THE ORDERING OF THESE VALUES.
enum ParseResultType {
  // Error parsing the protocol buffer from a string.
  PARSE_FROM_STRING_ERROR = 0,

  // No platform_type set in the response.
  NO_PLATFORM_TYPE_ERROR = 1,

  // No threat_entry_type set in the response.
  NO_THREAT_ENTRY_TYPE_ERROR = 2,

  // No threat_type set in the response.
  NO_THREAT_TYPE_ERROR = 3,

  // No state set in the response for one or more lists.
  NO_STATE_ERROR = 4,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  PARSE_RESULT_TYPE_MAX = 5
};

// Record parsing errors of an update result.
void RecordParseUpdateResult(ParseResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4Update.Parse.Result", result_type,
                            PARSE_RESULT_TYPE_MAX);
}

void RecordUpdateResult(safe_browsing::V4OperationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.V4Update.Result", result,
      safe_browsing::V4OperationResult::OPERATION_RESULT_MAX);
}

}  // namespace

namespace safe_browsing {

// Minimum time, in seconds, from start up before we must issue an update query.
static const int kV4TimerStartIntervalSecMin = 60;

// Maximum time, in seconds, from start up before we must issue an update query.
static const int kV4TimerStartIntervalSecMax = 300;

// Maximum time, in seconds, to wait for a response to an update request.
static const int kV4TimerUpdateWaitSecMax = 30;

ChromeClientInfo::SafeBrowsingReportingPopulation GetReportingLevelProtoValue(
    ExtendedReportingLevel reporting_level) {
  switch (reporting_level) {
    case SBER_LEVEL_OFF:
      return ChromeClientInfo::OPT_OUT;
    case SBER_LEVEL_LEGACY:
      return ChromeClientInfo::EXTENDED;
    case SBER_LEVEL_SCOUT:
      return ChromeClientInfo::SCOUT;
    default:
      NOTREACHED() << "Unexpected reporting_level!";
      return ChromeClientInfo::UNSPECIFIED;
  }
}

// The default V4UpdateProtocolManagerFactory.
class V4UpdateProtocolManagerFactoryImpl
    : public V4UpdateProtocolManagerFactory {
 public:
  V4UpdateProtocolManagerFactoryImpl() {}
  ~V4UpdateProtocolManagerFactoryImpl() override {}
  std::unique_ptr<V4UpdateProtocolManager> CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config,
      V4UpdateCallback update_callback,
      ExtendedReportingLevelCallback extended_reporting_level_callback)
      override {
    return std::unique_ptr<V4UpdateProtocolManager>(new V4UpdateProtocolManager(
        request_context_getter, config, update_callback,
        extended_reporting_level_callback));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(V4UpdateProtocolManagerFactoryImpl);
};

// V4UpdateProtocolManager implementation --------------------------------

// static
V4UpdateProtocolManagerFactory* V4UpdateProtocolManager::factory_ = NULL;

// static
std::unique_ptr<V4UpdateProtocolManager> V4UpdateProtocolManager::Create(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config,
    V4UpdateCallback update_callback,
    ExtendedReportingLevelCallback extended_reporting_level_callback) {
  if (!factory_) {
    factory_ = new V4UpdateProtocolManagerFactoryImpl();
  }
  return factory_->CreateProtocolManager(request_context_getter, config,
                                         update_callback,
                                         extended_reporting_level_callback);
}

void V4UpdateProtocolManager::ResetUpdateErrors() {
  update_error_count_ = 0;
  update_back_off_mult_ = 1;
}

V4UpdateProtocolManager::V4UpdateProtocolManager(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config,
    V4UpdateCallback update_callback,
    ExtendedReportingLevelCallback extended_reporting_level_callback)
    : update_error_count_(0),
      update_back_off_mult_(1),
      next_update_interval_(base::TimeDelta::FromSeconds(
          base::RandInt(kV4TimerStartIntervalSecMin,
                        kV4TimerStartIntervalSecMax))),
      config_(config),
      request_context_getter_(request_context_getter),
      url_fetcher_id_(0),
      update_callback_(update_callback),
      extended_reporting_level_callback_(extended_reporting_level_callback) {
  // Do not auto-schedule updates. Let the owner (V4LocalDatabaseManager) do it
  // when it is ready to process updates.
}

V4UpdateProtocolManager::~V4UpdateProtocolManager() {}

bool V4UpdateProtocolManager::IsUpdateScheduled() const {
  return update_timer_.IsRunning();
}

void V4UpdateProtocolManager::ScheduleNextUpdate(
    std::unique_ptr<StoreStateMap> store_state_map) {
  store_state_map_ = std::move(store_state_map);
  ScheduleNextUpdateWithBackoff(false);
}

void V4UpdateProtocolManager::ScheduleNextUpdateWithBackoff(bool back_off) {
  DCHECK(CalledOnValidThread());

  if (config_.disable_auto_update) {
    DCHECK(!IsUpdateScheduled());
    return;
  }

  // Reschedule with the new update.
  base::TimeDelta next_update_interval = GetNextUpdateInterval(back_off);
  ScheduleNextUpdateAfterInterval(next_update_interval);
}

// According to section 5 of the SafeBrowsing protocol specification, we must
// back off after a certain number of errors.
base::TimeDelta V4UpdateProtocolManager::GetNextUpdateInterval(bool back_off) {
  DCHECK(CalledOnValidThread());
  DCHECK(next_update_interval_ > base::TimeDelta());

  base::TimeDelta next = next_update_interval_;
  if (back_off) {
    next = V4ProtocolManagerUtil::GetNextBackOffInterval(
        &update_error_count_, &update_back_off_mult_);
  }

  if (!last_response_time_.is_null()) {
    // The callback spent some time updating the database, including disk I/O.
    // Do not wait that extra time.
    base::TimeDelta callback_time = Time::Now() - last_response_time_;
    if (callback_time < next) {
      next -= callback_time;
    } else {
      // If the callback took too long, schedule the next update with no delay.
      next = base::TimeDelta();
    }
  }
  DVLOG(1) << "V4UpdateProtocolManager::GetNextUpdateInterval: "
           << "next_interval: " << next;
  return next;
}

void V4UpdateProtocolManager::ScheduleNextUpdateAfterInterval(
    base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  DCHECK(interval >= base::TimeDelta());

  // Unschedule any current timer.
  update_timer_.Stop();
  update_timer_.Start(FROM_HERE, interval, this,
                      &V4UpdateProtocolManager::IssueUpdateRequest);
}

std::string V4UpdateProtocolManager::GetBase64SerializedUpdateRequestProto() {
  DCHECK(!store_state_map_->empty());
  // Build the request. Client info and client states are not added to the
  // request protocol buffer. Client info is passed as params in the url.
  FetchThreatListUpdatesRequest request;
  for (const auto& entry : *store_state_map_) {
    const auto& list_to_update = entry.first;
    const auto& state = entry.second;
    ListUpdateRequest* list_update_request = request.add_list_update_requests();
    list_update_request->set_platform_type(list_to_update.platform_type());
    list_update_request->set_threat_entry_type(
        list_to_update.threat_entry_type());
    list_update_request->set_threat_type(list_to_update.threat_type());

    if (!state.empty()) {
      list_update_request->set_state(state);
    }

    list_update_request->mutable_constraints()->add_supported_compressions(RAW);
    list_update_request->mutable_constraints()->add_supported_compressions(
        RICE);
  }

  if (!extended_reporting_level_callback_.is_null()) {
    request.mutable_chrome_client_info()->set_reporting_population(
        GetReportingLevelProtoValue(extended_reporting_level_callback_.Run()));
  }

  V4ProtocolManagerUtil::SetClientInfoFromConfig(request.mutable_client(),
                                                 config_);

  // Serialize and Base64 encode.
  std::string req_data, req_base64;
  request.SerializeToString(&req_data);
  base::Base64UrlEncode(req_data, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &req_base64);
  return req_base64;
}

bool V4UpdateProtocolManager::ParseUpdateResponse(
    const std::string& data,
    ParsedServerResponse* parsed_server_response) {
  FetchThreatListUpdatesResponse response;

  if (!response.ParseFromString(data)) {
    RecordParseUpdateResult(PARSE_FROM_STRING_ERROR);
    return false;
  }

  if (response.has_minimum_wait_duration()) {
    // Seconds resolution is good enough so we ignore the nanos field.
    int64_t minimum_wait_duration_seconds =
        response.minimum_wait_duration().seconds();

    // Do not let the next_update_interval_ to be too low.
    if (minimum_wait_duration_seconds < kV4TimerStartIntervalSecMin) {
      minimum_wait_duration_seconds = kV4TimerStartIntervalSecMin;
    }
    next_update_interval_ =
        base::TimeDelta::FromSeconds(minimum_wait_duration_seconds);
  }

  for (ListUpdateResponse& list_update_response :
       *response.mutable_list_update_responses()) {
    if (!list_update_response.has_platform_type()) {
      RecordParseUpdateResult(NO_PLATFORM_TYPE_ERROR);
    } else if (!list_update_response.has_threat_entry_type()) {
      RecordParseUpdateResult(NO_THREAT_ENTRY_TYPE_ERROR);
    } else if (!list_update_response.has_threat_type()) {
      RecordParseUpdateResult(NO_THREAT_TYPE_ERROR);
    } else if (!list_update_response.has_new_client_state()) {
      RecordParseUpdateResult(NO_STATE_ERROR);
    } else {
      std::unique_ptr<ListUpdateResponse> add(new ListUpdateResponse);
      add->Swap(&list_update_response);
      parsed_server_response->push_back(std::move(add));
    }
  }
  return true;
}

void V4UpdateProtocolManager::IssueUpdateRequest() {
  DCHECK(CalledOnValidThread());

  // If an update request is already pending, record and return silently.
  if (request_.get()) {
    RecordUpdateResult(V4OperationResult::ALREADY_PENDING_ERROR);
    return;
  }

  std::string req_base64 = GetBase64SerializedUpdateRequestProto();
  GURL update_url;
  net::HttpRequestHeaders headers;
  GetUpdateUrlAndHeaders(req_base64, &update_url, &headers);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_g4_update", R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "Safe Browsing issues a request to Google every 30 minutes or so "
            "to get the latest database of hashes of bad URLs."
          trigger:
            "On a timer, approximately every 30 minutes."
          data:
             "The state of the local DB is sent so the server can send just "
             "the changes. This doesn't include any user data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: true
          cookies_store: "Safe Browsing cookie store"
          setting:
            "Users can disable Safe Browsing by unchecking 'Protect you and "
            "your device from dangerous sites' in Chromium settings under "
            "Privacy. The feature is enabled by default."
          chrome_policy {
            SafeBrowsingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingEnabled: false
            }
          }
        })");
  std::unique_ptr<net::URLFetcher> fetcher =
      net::URLFetcher::Create(url_fetcher_id_++, update_url,
                              net::URLFetcher::GET, this, traffic_annotation);
  fetcher->SetExtraRequestHeaders(headers.ToString());
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher.get(), data_use_measurement::DataUseUserData::SAFE_BROWSING);

  request_ = std::move(fetcher);

  request_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  request_->SetRequestContext(request_context_getter_.get());
  request_->Start();

  // Begin the update request timeout.
  timeout_timer_.Start(FROM_HERE,
                       TimeDelta::FromSeconds(kV4TimerUpdateWaitSecMax), this,
                       &V4UpdateProtocolManager::HandleTimeout);
}

void V4UpdateProtocolManager::HandleTimeout() {
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.V4Update.TimedOut", true);
  request_.reset();
  ScheduleNextUpdateWithBackoff(false);
}

// net::URLFetcherDelegate implementation ----------------------------------

// SafeBrowsing request responses are handled here.
void V4UpdateProtocolManager::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());

  timeout_timer_.Stop();

  int response_code = source->GetResponseCode();
  net::URLRequestStatus status = source->GetStatus();
  V4ProtocolManagerUtil::RecordHttpResponseOrErrorCode(
      "SafeBrowsing.V4Update.Network.Result", status, response_code);
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.V4Update.TimedOut", false);

  last_response_time_ = Time::Now();

  std::unique_ptr<ParsedServerResponse> parsed_server_response(
      new ParsedServerResponse);
  if (status.is_success() && response_code == net::HTTP_OK) {
    RecordUpdateResult(V4OperationResult::STATUS_200);
    ResetUpdateErrors();
    std::string data;
    source->GetResponseAsString(&data);
    if (!ParseUpdateResponse(data, parsed_server_response.get())) {
      parsed_server_response->clear();
      RecordUpdateResult(V4OperationResult::PARSE_ERROR);
    }
    request_.reset();

    UMA_HISTOGRAM_COUNTS("SafeBrowsing.V4Update.ResponseSizeKB",
                         data.size() / 1024);

    // The caller should update its state now, based on parsed_server_response.
    // The callback must call ScheduleNextUpdate() at the end to resume
    // downloading updates.
    update_callback_.Run(std::move(parsed_server_response));
  } else {
    DVLOG(1) << "SafeBrowsing GetEncodedUpdates request for: "
             << source->GetURL() << " failed with error: " << status.error()
             << " and response code: " << response_code;

    if (status.status() == net::URLRequestStatus::FAILED) {
      RecordUpdateResult(V4OperationResult::NETWORK_ERROR);
    } else {
      RecordUpdateResult(V4OperationResult::HTTP_ERROR);
    }
    // TODO(vakh): Figure out whether it is just a network error vs backoff vs
    // another condition and RecordUpdateResult more accurately.

    request_.reset();
    ScheduleNextUpdateWithBackoff(true);
  }
}

void V4UpdateProtocolManager::GetUpdateUrlAndHeaders(
    const std::string& req_base64,
    GURL* gurl,
    net::HttpRequestHeaders* headers) const {
  V4ProtocolManagerUtil::GetRequestUrlAndHeaders(
      req_base64, "threatListUpdates:fetch", config_, gurl, headers);
}

}  // namespace safe_browsing
