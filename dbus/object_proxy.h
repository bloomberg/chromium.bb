// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_OBJECT_PROXY_H_
#define DBUS_OBJECT_PROXY_H_
#pragma once

#include <dbus/dbus.h>

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"

namespace dbus {

class Bus;
class MethodCall;
class Response;
class Signal;

// ObjectProxy is used to communicate with remote objects, mainly for
// calling methods of these objects.
//
// ObjectProxy is a ref counted object, to ensure that |this| of the
// object is is alive when callbacks referencing |this| are called.
class ObjectProxy : public base::RefCountedThreadSafe<ObjectProxy> {
 public:
  // Client code should use Bus::GetObjectProxy() instead of this
  // constructor.
  ObjectProxy(Bus* bus,
              const std::string& service_name,
              const std::string& object_path);

  // Special timeout constants.
  //
  // The constants correspond to DBUS_TIMEOUT_USE_DEFAULT and
  // DBUS_TIMEOUT_INFINITE. Here we use literal numbers instead of these
  // macros as these aren't defined with D-Bus earlier than 1.4.12.
  enum {
    TIMEOUT_USE_DEFAULT = -1,
    TIMEOUT_INFINITE = 0x7fffffff,
  };

  // Called when the response is returned. Used for CallMethod().
  typedef base::Callback<void(Response*)> ResponseCallback;

  // Called when a signal is received. Signal* is the incoming signal.
  typedef base::Callback<void (Signal*)> SignalCallback;

  // Called when the object proxy is connected to the signal.
  // Parameters:
  // - the interface name.
  // - the signal name.
  // - whether it was successful or not.
  typedef base::Callback<void (const std::string&, const std::string&, bool)>
      OnConnectedCallback;

  // Calls the method of the remote object and blocks until the response
  // is returned. Returns NULL on error.
  //
  // BLOCKING CALL.
  virtual Response* CallMethodAndBlock(MethodCall* method_call,
                                       int timeout_ms);

  // Requests to call the method of the remote object.
  //
  // |callback| will be called in the origin thread, once the method call
  // is complete. As it's called in the origin thread, |callback| can
  // safely reference objects in the origin thread (i.e. UI thread in most
  // cases). If the caller is not interested in the response from the
  // method (i.e. calling a method that does not return a value),
  // EmptyResponseCallback() can be passed to the |callback| parameter.
  //
  // If the method call is successful, a pointer to Response object will
  // be passed to the callback. If unsuccessful, NULL will be passed to
  // the callback.
  //
  // Must be called in the origin thread.
  virtual void CallMethod(MethodCall* method_call,
                          int timeout_ms,
                          ResponseCallback callback);

  // Requests to connect to the signal from the remote object.
  //
  // |signal_callback| will be called in the origin thread, when the
  // signal is received from the remote object. As it's called in the
  // origin thread, |signal_callback| can safely reference objects in the
  // origin thread (i.e. UI thread in most cases).
  //
  // |on_connected_callback| is called when the object proxy is connected
  // to the signal, or failed to be connected, in the origin thread.
  //
  // Must be called in the origin thread.
  virtual void ConnectToSignal(const std::string& interface_name,
                               const std::string& signal_name,
                               SignalCallback signal_callback,
                               OnConnectedCallback on_connected_callback);

  // Detaches from the remote object. The Bus object will take care of
  // detaching so you don't have to do this manually.
  //
  // BLOCKING CALL.
  virtual void Detach();

  // Returns an empty callback that does nothing. Can be used for
  // CallMethod().
  static ResponseCallback EmptyResponseCallback();

 protected:
  // This is protected, so we can define sub classes.
  virtual ~ObjectProxy();

 private:
  friend class base::RefCountedThreadSafe<ObjectProxy>;

  // Struct of data we'll be passing from StartAsyncMethodCall() to
  // OnPendingCallIsCompleteThunk().
  struct OnPendingCallIsCompleteData {
    OnPendingCallIsCompleteData(ObjectProxy* in_object_proxy,
                                ResponseCallback in_response_callback,
                                base::TimeTicks start_time);
    ~OnPendingCallIsCompleteData();

    ObjectProxy* object_proxy;
    ResponseCallback response_callback;
    base::TimeTicks start_time;
  };

  // Starts the async method call. This is a helper function to implement
  // CallMethod().
  void StartAsyncMethodCall(int timeout_ms,
                            DBusMessage* request_message,
                            ResponseCallback response_callback,
                            base::TimeTicks start_time);

  // Called when the pending call is complete.
  void OnPendingCallIsComplete(DBusPendingCall* pending_call,
                               ResponseCallback response_callback,
                               base::TimeTicks start_time);

  // Runs the response callback with the given response object.
  void RunResponseCallback(ResponseCallback response_callback,
                           base::TimeTicks start_time,
                           DBusMessage* response_message);

  // Redirects the function call to OnPendingCallIsComplete().
  static void OnPendingCallIsCompleteThunk(DBusPendingCall* pending_call,
                                           void* user_data);

  // Helper function for ConnectToSignal().
  void ConnectToSignalInternal(
      const std::string& interface_name,
      const std::string& signal_name,
      SignalCallback signal_callback,
      OnConnectedCallback on_connected_callback);

  // Called when the object proxy is connected to the signal, or failed.
  void OnConnected(OnConnectedCallback on_connected_callback,
                   const std::string& interface_name,
                   const std::string& signal_name,
                   bool success);

  // Handles the incoming request messages and dispatches to the signal
  // callbacks.
  DBusHandlerResult HandleMessage(DBusConnection* connection,
                                  DBusMessage* raw_message);

  // Runs the method. Helper function for HandleMessage().
  void RunMethod(base::TimeTicks start_time,
                 SignalCallback signal_callback,
                 Signal* signal);

  // Redirects the function call to HandleMessage().
  static DBusHandlerResult HandleMessageThunk(DBusConnection* connection,
                                              DBusMessage* raw_message,
                                              void* user_data);

  scoped_refptr<Bus> bus_;
  std::string service_name_;
  std::string object_path_;

  // True if the message filter was added.
  bool filter_added_;

  // The method table where keys are absolute signal names (i.e. interface
  // name + signal name), and values are the corresponding callbacks.
  typedef std::map<std::string, SignalCallback> MethodTable;
  MethodTable method_table_;

  std::set<std::string> match_rules_;

  DISALLOW_COPY_AND_ASSIGN(ObjectProxy);
};

}  // namespace dbus

#endif  // DBUS_OBJECT_PROXY_H_
