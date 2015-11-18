// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_CONNECTION_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_CONNECTION_DELEGATE_H_

#include "base/callback.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/api/display_source.h"

namespace extensions {

using DisplaySourceSinkInfoPtr = linked_ptr<api::display_source::SinkInfo>;
using DisplaySourceSinkInfoList = std::vector<DisplaySourceSinkInfoPtr>;
using DisplaySourceAuthInfo = api::display_source::AuthenticationInfo;

// The DisplaySourceConnectionDelegate interface should be implemented
// to provide sinks search and connection functionality for
// 'chrome.displaySource'
// API.
class DisplaySourceConnectionDelegate : public KeyedService {
 public:
  using AuthInfoCallback = base::Callback<void(const DisplaySourceAuthInfo&)>;
  using FailureCallback = base::Callback<void(const std::string&)>;
  using SinkInfoListCallback =
      base::Callback<void(const DisplaySourceSinkInfoList&)>;

  struct Connection {
    Connection();
    ~Connection();
    DisplaySourceSinkInfoPtr connected_sink;
    std::string local_ip;
    std::string sink_ip;
  };

  class Observer {
   public:
    // This method is called each tiome the list of available
    // sinks is updated whether after 'GetAvailableSinks' call
    // or while the implementation is constantly watching the sinks
    // (after 'StartWatchingSinks' was called).
    virtual void OnSinksUpdated(const DisplaySourceSinkInfoList& sinks) = 0;

   protected:
    virtual ~Observer() {}
  };

  DisplaySourceConnectionDelegate();
  ~DisplaySourceConnectionDelegate() override;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Returns the list of last found available sinks
  // this list may contain outdated data if the delegate
  // is not watching the sinks (via 'StartWatchingSinks'
  // method). The list is refreshed after 'GetAvailableSinks'
  // call.
  virtual DisplaySourceSinkInfoList last_found_sinks() const = 0;

  // Returns the Connection object representing the current
  // connection to the sink or NULL if there is no curent connection.
  virtual const Connection* connection() const = 0;

  // Queries the list of currently available sinks.
  virtual void GetAvailableSinks(const SinkInfoListCallback& sinks_callback,
                                 const FailureCallback& failure_callback) = 0;

  // Queries the authentication method required by the sink for connection.
  // If the used authentication method requires authentication data to be
  // visible on the sink's display (e.g. PIN) the implementation should
  // request the sink to show it.
  virtual void RequestAuthentication(
      int sink_id,
      const AuthInfoCallback& auth_info_callback,
      const FailureCallback& failure_callback) = 0;

  // Connects to a sink by given id and auth info.
  virtual void Connect(int sink_id,
                       const DisplaySourceAuthInfo& auth_info,
                       const base::Closure& connected_callback,
                       const FailureCallback& failure_callback) = 0;

  // Disconnects the current connection to sink, the 'failure_callback'
  // is called if there is no current connection.
  virtual void Disconnect(const base::Closure& disconnected_callback,
                          const FailureCallback& failure_callback) = 0;

  // Implementation should start watching the sinks updates.
  virtual void StartWatchingSinks() = 0;

  // Implementation should stop watching the sinks updates.
  virtual void StopWatchingSinks() = 0;

 protected:
  base::ObserverList<Observer> observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_CONNECTION_DELEGATE_H_
