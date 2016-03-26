// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_update_protocol_manager.h"

#include <utility>

#include "base/base64.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
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
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.ParseV4UpdateResult", result_type,
                            PARSE_RESULT_TYPE_MAX);
}

void RecordUpdateResult(safe_browsing::V4OperationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.V4UpdateResult", result,
      safe_browsing::V4OperationResult::OPERATION_RESULT_MAX);
}

}  // namespace

namespace safe_browsing {

// The default V4UpdateProtocolManagerFactory.
class V4UpdateProtocolManagerFactoryImpl
    : public V4UpdateProtocolManagerFactory {
 public:
  V4UpdateProtocolManagerFactoryImpl() {}
  ~V4UpdateProtocolManagerFactoryImpl() override {}
  V4UpdateProtocolManager* CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config) override {
    return new V4UpdateProtocolManager(request_context_getter, config);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(V4UpdateProtocolManagerFactoryImpl);
};

// V4UpdateProtocolManager implementation --------------------------------

// static
V4UpdateProtocolManagerFactory* V4UpdateProtocolManager::factory_ = NULL;

// static
V4UpdateProtocolManager* V4UpdateProtocolManager::Create(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  if (!factory_)
    factory_ = new V4UpdateProtocolManagerFactoryImpl();
  return factory_->CreateProtocolManager(request_context_getter, config);
}

void V4UpdateProtocolManager::ResetUpdateErrors() {
  update_error_count_ = 0;
  update_back_off_mult_ = 1;
}

V4UpdateProtocolManager::V4UpdateProtocolManager(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config)
    : update_error_count_(0),
      update_back_off_mult_(1),
      next_update_time_(Time::Now()),
      config_(config),
      request_context_getter_(request_context_getter),
      url_fetcher_id_(0) {}

V4UpdateProtocolManager::~V4UpdateProtocolManager() {
}

std::string V4UpdateProtocolManager::GetUpdateRequest(
    const base::hash_set<UpdateListIdentifier>& lists_to_update,
    const base::hash_map<UpdateListIdentifier, std::string>&
        current_list_states) {
  // Build the request. Client info and client states are not added to the
  // request protocol buffer. Client info is passed as params in the url.
  FetchThreatListUpdatesRequest request;
  for (const auto& list_to_update : lists_to_update) {
    ListUpdateRequest* list_update_request = request.add_list_update_requests();
    list_update_request->set_platform_type(list_to_update.platform_type);
    list_update_request->set_threat_entry_type(
        list_to_update.threat_entry_type);
    list_update_request->set_threat_type(list_to_update.threat_type);

    // If the current state of the list is available, add that to the proto.
    base::hash_map<UpdateListIdentifier, std::string>::const_iterator
        list_iter = current_list_states.find(list_to_update);
    if (list_iter != current_list_states.end()) {
      list_update_request->set_state(list_iter->second);
    }
  }

  // Serialize and Base64 encode.
  std::string req_data, req_base64;
  request.SerializeToString(&req_data);
  base::Base64Encode(req_data, &req_base64);

  return req_base64;
}

bool V4UpdateProtocolManager::ParseUpdateResponse(
    const std::string& data,
    std::vector<ListUpdateResponse>* list_update_responses) {
  FetchThreatListUpdatesResponse response;

  if (!response.ParseFromString(data)) {
    RecordParseUpdateResult(PARSE_FROM_STRING_ERROR);
    return false;
  }

  if (response.has_minimum_wait_duration()) {
    // Seconds resolution is good enough so we ignore the nanos field.
    base::TimeDelta next_update_interval = base::TimeDelta::FromSeconds(
        response.minimum_wait_duration().seconds());
    next_update_time_ = Time::Now() + next_update_interval;
  }

  // TODO(vakh): Do something useful with this response.
  for (const ListUpdateResponse& list_update_response :
       response.list_update_responses()) {
    if (!list_update_response.has_platform_type()) {
      RecordParseUpdateResult(NO_PLATFORM_TYPE_ERROR);
    } else if (!list_update_response.has_threat_entry_type()) {
      RecordParseUpdateResult(NO_THREAT_ENTRY_TYPE_ERROR);
    } else if (!list_update_response.has_threat_type()) {
      RecordParseUpdateResult(NO_THREAT_TYPE_ERROR);
    } else if (!list_update_response.has_new_client_state()) {
      RecordParseUpdateResult(NO_STATE_ERROR);
    } else {
      list_update_responses->push_back(list_update_response);
    }
  }
  return true;
}

void V4UpdateProtocolManager::GetUpdates(
    const base::hash_set<UpdateListIdentifier>& lists_to_update,
    const base::hash_map<UpdateListIdentifier, std::string>&
        current_list_states,
    UpdateCallback callback) {
  DCHECK(CalledOnValidThread());

  // If an update request is already pending, return an empty result.
  if (request_) {
    RecordUpdateResult(V4OperationResult::ALREADY_PENDING_ERROR);
    std::vector<ListUpdateResponse> list_update_responses;
    callback.Run(list_update_responses);
    return;
  }

  // We need to wait the minimum waiting duration, and if we are in backoff,
  // we need to check if we're past the next allowed time. If we are, we can
  // proceed with the request. If not, we are required to return empty results.
  if (Time::Now() <= next_update_time_) {
    if (update_error_count_) {
      RecordUpdateResult(V4OperationResult::BACKOFF_ERROR);
    } else {
      RecordUpdateResult(V4OperationResult::MIN_WAIT_DURATION_ERROR);
    }
    std::vector<ListUpdateResponse> list_update_responses;
    callback.Run(list_update_responses);
    return;
  }

  std::string req_base64 =
      GetUpdateRequest(lists_to_update, current_list_states);
  GURL update_url = GetUpdateUrl(req_base64);

  request_.reset(net::URLFetcher::Create(url_fetcher_id_++, update_url,
                                         net::URLFetcher::GET, this)
                     .release());
  callback_ = callback;

  request_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  request_->SetRequestContext(request_context_getter_.get());
  request_->Start();
  //TODO(vakh): Handle request timeout.
}

// net::URLFetcherDelegate implementation ----------------------------------

// SafeBrowsing request responses are handled here.
void V4UpdateProtocolManager::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());

  int response_code = source->GetResponseCode();
  net::URLRequestStatus status = source->GetStatus();
  V4ProtocolManagerUtil::RecordHttpResponseOrErrorCode(
    "SafeBrowsing.V4UpdateHttpResponseOrErrorCode", status, response_code);

  std::vector<ListUpdateResponse> list_update_responses;
  if (status.is_success() && response_code == net::HTTP_OK) {
    RecordUpdateResult(V4OperationResult::STATUS_200);
    ResetUpdateErrors();
    std::string data;
    source->GetResponseAsString(&data);
    if (!ParseUpdateResponse(data, &list_update_responses)) {
      list_update_responses.clear();
      RecordUpdateResult(V4OperationResult::PARSE_ERROR);
    }
  } else {
    HandleUpdateError(Time::Now());

    DVLOG(1) << "SafeBrowsing GetEncodedUpdates request for: "
             << source->GetURL() << " failed with error: " << status.error()
             << " and response code: " << response_code;

    if (status.status() == net::URLRequestStatus::FAILED) {
      RecordUpdateResult(V4OperationResult::NETWORK_ERROR);
    } else {
      RecordUpdateResult(V4OperationResult::HTTP_ERROR);
    }
  }

  // Invoke the callback with list_update_responses, even if there was a parse
  // error or an error response code (in which case list_update_responses will
  // be empty). The caller can't be blocked indefinitely.
  callback_.Run(list_update_responses);
  request_.reset();
}

void V4UpdateProtocolManager::HandleUpdateError(const Time& now) {
  DCHECK(CalledOnValidThread());
  base::TimeDelta next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &update_error_count_, &update_back_off_mult_);
  next_update_time_ = now + next;
}

GURL V4UpdateProtocolManager::GetUpdateUrl(
    const std::string& req_base64) const {
  return V4ProtocolManagerUtil::GetRequestUrl(req_base64, "encodedUpdates",
                                              config_);
}

}  // namespace safe_browsing
