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

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
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

class V4UpdateProtocolManager : public net::URLFetcherDelegate,
                                public base::NonThreadSafe {
 public:
  typedef FetchThreatListUpdatesRequest::ListUpdateRequest ListUpdateRequest;
  typedef FetchThreatListUpdatesResponse::ListUpdateResponse ListUpdateResponse;

  // UpdateCallback is invoked when GetUpdates completes.
  // Parameters:
  //   - The vector of update response protobufs received from the server for
  //     each list type.
  //  The caller can then use this vector to re-build the current_list_states.
  typedef base::Callback<void(const std::vector<ListUpdateResponse>&)>
      UpdateCallback;

  ~V4UpdateProtocolManager() override;

  // Makes the passed |factory| the factory used to instantiate
  // a V4UpdateProtocolManager. Useful for tests.
  static void RegisterFactory(V4UpdateProtocolManagerFactory* factory) {
    factory_ = factory;
  }

  // Create an instance of the safe browsing v4 protocol manager.
  static V4UpdateProtocolManager* Create(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config);

  // net::URLFetcherDelegate interface.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Retrieve the hash prefix update, and invoke the callback argument when the
  // results are retrieved. The callback may be invoked synchronously.
  // Parameters:
  //   - The set of lists to fetch the updates for.
  //   - The last known state for each of the known lists.
  // It is valid to have one or more lists in lists_to_update set that have no
  // corresponding value in the current_list_states map. This corresponds to the
  // initial state for those lists.
  virtual void GetUpdates(
      const base::hash_set<UpdateListIdentifier>& lists_to_update,
      const base::hash_map<UpdateListIdentifier, std::string>&
          current_list_states,
      UpdateCallback callback);

 protected:
  // Constructs a V4UpdateProtocolManager that issues network requests using
  // |request_context_getter|.
  V4UpdateProtocolManager(net::URLRequestContextGetter* request_context_getter,
                          const V4ProtocolConfig& config);

 private:
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesErrorHandlingNetwork);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesErrorHandlingResponseCode);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest, TestGetUpdatesNoError);
  friend class V4UpdateProtocolManagerFactoryImpl;

  // The method to generate the URL for the request to be sent to the server.
  // |request_base64| is the base64 encoded form of an instance of the protobuf
  // FetchThreatListUpdatesRequest.
  GURL GetUpdateUrl(const std::string& request_base64) const;

  // Fills a FetchThreatListUpdatesRequest protocol buffer for a request.
  // Returns the serialized and base 64 encoded request as a string.
  std::string GetUpdateRequest(
      const base::hash_set<UpdateListIdentifier>& lists_to_update,
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

  // The factory that controls the creation of V4UpdateProtocolManager.
  // This is used by tests.
  static V4UpdateProtocolManagerFactory* factory_;

  // The number of HTTP response errors since the the last successful HTTP
  // response, used for request backoff timing.
  size_t update_error_count_;

  // Multiplier for the backoff error after the second.
  size_t update_back_off_mult_;

  // The time before which the next update request may not be sent.
  // It is set to:
  // the backoff time, if the last response was an error, or
  // the minimum wait time, if the last response was successful.
  base::Time next_update_time_;

  // The config of the client making Pver4 requests.
  const V4ProtocolConfig config_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // ID for URLFetchers for testing.
  int url_fetcher_id_;

  // True if there's a request pending.
  bool update_request_pending_;

  // The callback that's called when GetUpdates completes.
  UpdateCallback callback_;

  // The pending update request. The request must be canceled when the object is
  // destroyed.
  scoped_ptr<net::URLFetcher> request_;

  DISALLOW_COPY_AND_ASSIGN(V4UpdateProtocolManager);
};

// Interface of a factory to create V4UpdateProtocolManager.  Useful for tests.
class V4UpdateProtocolManagerFactory {
 public:
  V4UpdateProtocolManagerFactory() {}
  virtual ~V4UpdateProtocolManagerFactory() {}
  virtual V4UpdateProtocolManager* CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(V4UpdateProtocolManagerFactory);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_UPDATE_PROTOCOL_MANAGER_H_
