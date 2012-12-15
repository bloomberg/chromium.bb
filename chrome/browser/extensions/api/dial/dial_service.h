// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_SERVICE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "net/base/net_log.h"
#include "net/udp/udp_socket.h"

namespace net {
class IPEndPoint;
class IPAddress;
class IOBuffer;
class StringIOBuffer;
struct NetworkInterface;
}

namespace extensions {

class DialDeviceData;

// DialService accepts requests to discover devices, sends multiple M-SEARCH
// requests via UDP multicast, and notifies observers when a DIAL-compliant
// device responds.
//
// Each time Discover() is called, kDialNumRequests M-SEARCH requests are sent
// (with a delay of kDialRequestIntervalMillis in between):
//
// Time    Action
// ----    ------
// T1      Request 1 sent, OnDiscoveryReqest() called
// ...
// Tk      Request kDialNumRequests sent, OnDiscoveryReqest() called
// Tf      OnDiscoveryFinished() called
//
// Any time a valid response is received between T1 and Tf, it is parsed and
// OnDeviceDiscovered() is called with the result.  Tf is set to Tk +
// kDialResponseTimeoutSecs (the response timeout passed in each request).
//
// Calling Discover() again between T1 and Tf has no effect.
//
// All relevant constants are defined in dial_service.cc.
//
// TODO(mfoltz): Port this into net/.
// See https://code.google.com/p/chromium/issues/detail?id=164473
class DialService : public base::RefCountedThreadSafe<DialService> {
 public:
  class Observer {
   public:
    // Called when a single discovery request was sent.
    virtual void OnDiscoveryRequest(DialService* service) = 0;

    // Called when a device responds to a request.
    virtual void OnDeviceDiscovered(DialService* service,
                                    const DialDeviceData& device) = 0;

    // Called when we have all responses from the last discovery request.
    virtual void OnDiscoveryFinished(DialService* service) = 0;

    // Called when an error occurs.
    virtual void OnError(DialService* service, const std::string& msg) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Starts a new round of discovery.  Returns |true| if discovery was started
  // successfully or there is already one active. Returns |false| on error.
  virtual bool Discover() = 0;

  // Called by listeners to this service to add/remove themselves as observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(Observer* observer) = 0;

 protected:
  virtual ~DialService() {}

 private:
  friend class base::RefCountedThreadSafe<DialService>;
};

class DialServiceImpl : public DialService {
 public:
  explicit DialServiceImpl(net::NetLog* net_log);

  // DialService implementation
  virtual bool Discover() OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool HasObserver(Observer* observer) OVERRIDE;

 private:
  virtual ~DialServiceImpl();

  // Starts the flow to construct and send a discovery request.
  void StartRequest();

  // Establishes the UDP socket that is used for requests and responses, then
  // sends a discovery request on the bound socket.  Returns |true| if
  // successful.
  bool BindAndWriteSocket(const net::NetworkInterface& bind_interface);

  // Callback invoked for socket writes.
  void OnSocketWrite(int result);

  // Method to get the network list on the FILE thread.
  void DoGetNetworkList();

  // Send the network list to IO thread.
  void SendNetworkList(const net::NetworkInterfaceList& list);

  // Establishes the callback to read from the socket.  Returns true if
  // successful.
  bool ReadSocket();

  // Callback invoked for socket reads.
  void OnSocketRead(int result);

  // Handles |bytes_read| bytes read from the socket and calls ReadSocket to
  // await the next response.
  void HandleResponse(int bytes_read);

  // Parses a response into a DialDeviceData object. If the DIAL response is
  // invalid or does not contain enough information, then the return
  // value will be false and |device| is not changed.
  static bool ParseResponse(const std::string& response,
                            const base::Time& response_time,
                            DialDeviceData* device);

  // Called from finish_timer_ when we are done with the current round of
  // discovery.
  void FinishDiscovery();

  // Closes the socket.
  void CloseSocket();

  // Checks the result of a socket operation.  If the result is an error, closes
  // the socket, notifies observers via OnError(), and returns |false|.  Returns
  // |true| otherwise.
  bool CheckResult(const char* operation, int result);

  // The UDP socket.
  scoped_ptr<net::UDPSocket> socket_;

  // The multicast address:port for search requests.
  net::IPEndPoint send_address_;

  // The NetLog for this service.
  net::NetLog* net_log_;

  // The NetLog source for this service.
  net::NetLog::Source net_log_source_;

  // Buffer for socket writes.
  scoped_refptr<net::StringIOBuffer> send_buffer_;

  // Marks whether there is an active write callback.
  bool is_writing_;

  // Buffer for socket reads.
  scoped_refptr<net::IOBufferWithSize> recv_buffer_;

  // The source of of the last socket read.
  net::IPEndPoint recv_address_;

  // Marks whether there is an active read callback.
  bool is_reading_;

  // True when we are currently doing discovery.
  bool discovery_active_;

  // The number of requests that have been sent in the current discovery.
  int num_requests_sent_;

  // Timer for finishing discovery.
  base::OneShotTimer<DialServiceImpl> finish_timer_;

  // The delay for |finish_timer_|; how long to wait for discovery to finish.
  base::TimeDelta finish_delay_;

  // List of observers.
  ObserverList<Observer> observer_list_;

  // Thread checker.
  base::ThreadChecker thread_checker_;

  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestOnDeviceDiscovered);
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestOnDiscoveryFinished);
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestOnDiscoveryRequest);
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestResponseParsing);
  DISALLOW_COPY_AND_ASSIGN(DialServiceImpl);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_SERVICE_H_
