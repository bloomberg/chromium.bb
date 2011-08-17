// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_OBJECT_PROXY_H_
#define DBUS_OBJECT_PROXY_H_
#pragma once

#include <string>
#include <vector>
#include <dbus/dbus.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"

class MessageLoop;

namespace dbus {

class Bus;
class MethodCall;
class Response;

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

  // Calls the method of the remote object and blocks until the response
  // is returned.
  //
  // BLOCKING CALL.
  virtual bool CallMethodAndBlock(MethodCall* method_call,
                                  int timeout_ms,
                                  Response* response);

  // Requests to call the method of the remote object.
  //
  // |callback| will be called in the origin thread, once the method call
  // is complete. As it's called in the origin thread, |callback| can
  // safely reference objects in the origin thread (i.e. UI thread in most
  // cases).
  //
  // If the method call is successful, a pointer to Response object will
  // be passed to the callback. If unsuccessful, NULL will be passed to
  // the callback.
  //
  // Must be called in the origin thread.
  virtual void CallMethod(MethodCall* method_call,
                          int timeout_ms,
                          ResponseCallback callback);

 private:
  friend class base::RefCountedThreadSafe<ObjectProxy>;
  virtual ~ObjectProxy();

  // Struct of data we'll be passing from StartAsyncMethodCall() to
  // OnPendingCallIsCompleteThunk().
  struct OnPendingCallIsCompleteData {
    OnPendingCallIsCompleteData(ObjectProxy* in_object_proxy,
                                ResponseCallback in_response_callback);
    ~OnPendingCallIsCompleteData();

    ObjectProxy* object_proxy;
    ResponseCallback response_callback;
  };

  // Starts the async method call. This is a helper function to implement
  // CallMethod().
  void StartAsyncMethodCall(int timeout_ms,
                            void* request_message,
                            ResponseCallback response_callback);

  // Called when the pending call is complete.
  void OnPendingCallIsComplete(DBusPendingCall* pending_call,
                               ResponseCallback response_callback);

  // Runs the response callback with the given response object.
  void RunResponseCallback(ResponseCallback response_callback,
                           Response* response);

  // Redirects the function call to OnPendingCallIsComplete().
  static void OnPendingCallIsCompleteThunk(DBusPendingCall* pending_call,
                                           void* user_data);

  Bus* bus_;
  std::string service_name_;
  std::string object_path_;

  DISALLOW_COPY_AND_ASSIGN(ObjectProxy);
};

}  // namespace dbus

#endif  // DBUS_OBJECT_PROXY_H_
