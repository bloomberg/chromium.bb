// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_UPDATE_PROTOCOL_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_UPDATE_PROTOCOL_MANAGER_H_

// A class that implements Chrome's interface with the SafeBrowsing V4 update
// protocol.
//
// The V4UpdateProtocolManager handles formatting and making requests of, and
// handling responses from, Google's SafeBrowsing servers. The purpose of this
// class is to get hash prefixes from the SB server for the given set of lists.

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "components/safe_browsing_db/util.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace safe_browsing {

class V4UpdateProtocolManagerFactory;

typedef FetchThreatListUpdatesRequest::ListUpdateRequest ListUpdateRequest;
typedef FetchThreatListUpdatesResponse::ListUpdateResponse ListUpdateResponse;

// V4UpdateCallback is invoked when a scheduled update completes.
// Parameters:
//   - The vector of update response protobufs received from the server for
//     each list type.
typedef base::Callback<void(const std::vector<ListUpdateResponse>&)>
    V4UpdateCallback;

class V4UpdateProtocolManager : public net::URLFetcherDelegate,
                                public base::NonThreadSafe {
 public:
  ~V4UpdateProtocolManager() override;

  // Makes the passed |factory| the factory used to instantiate
  // a V4UpdateProtocolManager. Useful for tests.
  static void RegisterFactory(V4UpdateProtocolManagerFactory* factory) {
    factory_ = factory;
  }

  // Create an instance of the safe browsing v4 protocol manager.
  static std::unique_ptr<V4UpdateProtocolManager> Create(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config,
      const base::hash_map<UpdateListIdentifier, std::string>&
          current_list_states,
      V4UpdateCallback callback);

  // net::URLFetcherDelegate interface.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 protected:
  // Constructs a V4UpdateProtocolManager that issues network requests using
  // |request_context_getter|.
  // Schedules an update to get the hash prefixes for the lists in
  // |current_list_states|, and invoke |callback| when the results
  // are retrieved. The callback may be invoked synchronously.
  V4UpdateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config,
      const base::hash_map<UpdateListIdentifier, std::string>&
          current_list_states,
      V4UpdateCallback callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesErrorHandlingNetwork);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesErrorHandlingResponseCode);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest, TestGetUpdatesNoError);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesWithOneBackoff);
  friend class V4UpdateProtocolManagerFactoryImpl;

  // The method to generate the URL for the request to be sent to the server.
  // |request_base64| is the base64 encoded form of an instance of the protobuf
  // FetchThreatListUpdatesRequest.
  GURL GetUpdateUrl(const std::string& request_base64) const;

  // Fills a FetchThreatListUpdatesRequest protocol buffer for a request.
  // Returns the serialized and base 64 encoded request as a string.
  std::string GetBase64SerializedUpdateRequestProto(
      const base::hash_map<UpdateListIdentifier, std::string>&
          current_list_states);

  // Parses the base64 encoded response received from the server as a
  // FetchThreatListUpdatesResponse protobuf and returns each of the
  // ListUpdateResponse protobufs contained in it as a vector.
  // Returns true if parsing is successful, false otherwise.
  bool ParseUpdateResponse(
      const std::string& data_base64,
      std::vector<ListUpdateResponse>* list_update_responses);

  // Resets the update error counter and multiplier.
  void ResetUpdateErrors();

  // Updates internal update and backoff state for each update response error,
  // assuming that the current time is |now|.
  void HandleUpdateError(const base::Time& now);

  // Generates the URL for the update request and issues the request for the
  // lists passed to the constructor.
  void IssueUpdateRequest();

  // Returns whether another update is currently scheduled.
  bool IsUpdateScheduled() const;

  // Schedule the next update, considering whether we are in backoff.
  void ScheduleNextUpdate(bool back_off);

  // Schedule the next update, after the given interval.
  void ScheduleNextUpdateAfterInterval(base::TimeDelta interval);

  // Get the next update interval, considering whether we are in backoff.
  base::TimeDelta GetNextUpdateInterval(bool back_off);

  // The factory that controls the creation of V4UpdateProtocolManager.
  // This is used by tests.
  static V4UpdateProtocolManagerFactory* factory_;

  // The last known state of the lists.
  // At init, this is read from the disk or is empty for no prior state.
  // Each successful update from the server contains a new state for each
  // requested list.
  base::hash_map<UpdateListIdentifier, std::string> current_list_states_;

  // The number of HTTP response errors since the the last successful HTTP
  // response, used for request backoff timing.
  size_t update_error_count_;

  // Multiplier for the backoff error after the second.
  size_t update_back_off_mult_;

  // The time delta after which the next update request may be sent.
  // It is set to a random interval between 60 and 300 seconds at start.
  // The server can set it by setting the minimum_wait_duration.
  base::TimeDelta next_update_interval_;

  // The config of the client making Pver4 requests.
  const V4ProtocolConfig config_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // ID for URLFetchers for testing.
  int url_fetcher_id_;

  // The callback that's called when GetUpdates completes.
  V4UpdateCallback callback_;

  // The pending update request. The request must be canceled when the object is
  // destroyed.
  std::unique_ptr<net::URLFetcher> request_;

  // Timer to setup the next update request.
  base::OneShotTimer update_timer_;

  DISALLOW_COPY_AND_ASSIGN(V4UpdateProtocolManager);
};

// Interface of a factory to create V4UpdateProtocolManager.  Useful for tests.
class V4UpdateProtocolManagerFactory {
 public:
  V4UpdateProtocolManagerFactory() {}
  virtual ~V4UpdateProtocolManagerFactory() {}
  virtual std::unique_ptr<V4UpdateProtocolManager> CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config,
      const base::hash_map<UpdateListIdentifier, std::string>&
          current_list_states,
      V4UpdateCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(V4UpdateProtocolManagerFactory);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_UPDATE_PROTOCOL_MANAGER_H_
