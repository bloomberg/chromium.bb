// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GEARS_GEOLOCATION_NETWORK_LOCATION_REQUEST_H__
#define GEARS_GEOLOCATION_NETWORK_LOCATION_REQUEST_H__

// TODO(joth): port to chromium
#if 0

#include <vector>
#include "gears/base/common/basictypes.h"  // For int64
#include "gears/base/common/common.h"
#include "gears/base/common/event.h"
#include "gears/blob/blob.h"
#include "gears/geolocation/geolocation.h"
#include "gears/geolocation/device_data_provider.h"
#include "gears/localserver/common/async_task.h"

// An implementation of an AsyncTask that takes a set of device data and sends
// it to a server to get a position fix. It performs formatting of the request
// and interpretation of the response.
class NetworkLocationRequest : public AsyncTask {
 public:
  friend class scoped_ptr<NetworkLocationRequest>;  // For use in Create().

  // Interface for receiving callbacks from a NetworkLocationRequest object.
  class ListenerInterface {
   public:
    // Updates the listener with a new position. server_error indicates whether
    // was a server or network error - either no response or a 500 error code.
    virtual void LocationResponseAvailable(
        const Position &position,
        bool server_error,
        const std::string16 &access_token) = 0;
  };

  // Creates the object and starts its worker thread running. Returns NULL if
  // creation or initialisation fails.
  static NetworkLocationRequest* Create(BrowsingContext *browsing_context,
                                        const std::string16 &url,
                                        const std::string16 &host_name,
                                        ListenerInterface *listener);
  bool MakeRequest(const std::string16 &access_token,
                   const RadioData &radio_data,
                   const WifiData &wifi_data,
                   bool request_address,
                   const std::string16 &address_language,
                   double latitude,
                   double longitude,
                   int64 timestamp);
  // This method aborts any pending request and instructs the worker thread to
  // terminate. The object is destructed once the thread terminates. This
  // method blocks until the AsyncTask::Run() implementation is complete, after
  // which the thread will not attempt to access external resources such as the
  // listener.
  void StopThreadAndDelete();

 private:
  // Private constructor and destructor. Callers should use Create() and
  // StopThreadAndDelete().
  NetworkLocationRequest(BrowsingContext *browsing_context,
                         const std::string16 &url,
                         const std::string16 &host_name,
                         ListenerInterface *listener);
  virtual ~NetworkLocationRequest() {}

  void MakeRequestImpl();

  // AsyncTask implementation.
  virtual void Run();

  static bool FormRequestBody(const std::string16 &host_name,
                              const std::string16 &access_token,
                              const RadioData &radio_data,
                              const WifiData &wifi_data,
                              bool request_address,
                              std::string16 address_language,
                              double latitude,
                              double longitude,
                              bool is_reverse_geocode,
                              scoped_refptr<BlobInterface> *blob);

  static void GetLocationFromResponse(bool http_post_result,
                                      int status_code,
                                      const std::string &response_body,
                                      int64 timestamp,
                                      const std::string16 &server_url,
                                      bool is_reverse_geocode,
                                      Position *position,
                                      std::string16 *access_token);

  int64 timestamp_;  // The timestamp of the data used to make the request.
  scoped_refptr<BlobInterface> post_body_;
  ListenerInterface *listener_;
  std::string16 url_;
  std::string16 host_name_;

  Mutex is_processing_response_mutex_;

  bool is_reverse_geocode_;

  Event thread_event_;
  bool is_shutting_down_;

#ifdef USING_CCTESTS
  // Uses FormRequestBody for testing.
  friend void TestGeolocationFormRequestBody(JsCallContext *context);
  // Uses GetLocationFromResponse for testing.
  friend void TestGeolocationGetLocationFromResponse(
      JsCallContext *context,
      JsRunnerInterface *js_runner);
#endif

  DISALLOW_EVIL_CONSTRUCTORS(NetworkLocationRequest);
};

#endif  // if 0

#endif  // GEARS_GEOLOCATION_NETWORK_LOCATION_REQUEST_H__
