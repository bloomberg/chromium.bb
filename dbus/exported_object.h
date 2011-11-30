// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_EXPORTED_OBJECT_H_
#define DBUS_EXPORTED_OBJECT_H_
#pragma once

#include <dbus/dbus.h>

#include <map>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"

namespace dbus {

class Bus;
class MethodCall;
class Response;
class Signal;

// ExportedObject is used to export objects and methods to other D-Bus
// clients.
//
// ExportedObject is a ref counted object, to ensure that |this| of the
// object is alive when callbacks referencing |this| are called.
class ExportedObject : public base::RefCountedThreadSafe<ExportedObject> {
 public:
  // Client code should use Bus::GetExportedObject() instead of this
  // constructor.
  ExportedObject(Bus* bus,
                 const std::string& service_name,
                 const std::string& object_path);

  // Called to send a response from an exported method. Response* is the
  // response message. Callers should pass a NULL Response* in the event
  // of an error that prevents the sending of a response.
  typedef base::Callback<void (Response*)> ResponseSender;

  // Called when an exported method is called. MethodCall* is the request
  // message. ResponseSender is the callback that should be used to send a
  // response.
  typedef base::Callback<void (MethodCall*, ResponseSender)> MethodCallCallback;

  // Called when method exporting is done.
  // Parameters:
  // - the interface name.
  // - the method name.
  // - whether exporting was successful or not.
  typedef base::Callback<void (const std::string&, const std::string&, bool)>
      OnExportedCallback;

  // Exports the method specified by |interface_name| and |method_name|,
  // and blocks until exporting is done. Returns true on success.
  //
  // |method_call_callback| will be called in the origin thread, when the
  // exported method is called. As it's called in the origin thread,
  // |method_callback| can safely reference objects in the origin thread
  // (i.e. UI thread in most cases).
  //
  // BLOCKING CALL.
  virtual bool ExportMethodAndBlock(const std::string& interface_name,
                                    const std::string& method_name,
                                    MethodCallCallback method_call_callback);

  // Requests to export the method specified by |interface_name| and
  // |method_name|. See Also ExportMethodAndBlock().
  //
  // |on_exported_callback| is called when the method is exported or
  // failed to be exported, in the origin thread.
  //
  // Must be called in the origin thread.
  virtual void ExportMethod(const std::string& interface_name,
                            const std::string& method_name,
                            MethodCallCallback method_call_callback,
                            OnExportedCallback on_exported_callback);

  // Requests to send the signal from this object. The signal will be sent
  // asynchronously from the message loop in the D-Bus thread.
  virtual void SendSignal(Signal* signal);

  // Unregisters the object from the bus. The Bus object will take care of
  // unregistering so you don't have to do this manually.
  //
  // BLOCKING CALL.
  virtual void Unregister();

 protected:
  // This is protected, so we can define sub classes.
  virtual ~ExportedObject();

 private:
  friend class base::RefCountedThreadSafe<ExportedObject>;

  // Helper function for ExportMethod().
  void ExportMethodInternal(const std::string& interface_name,
                            const std::string& method_name,
                            MethodCallCallback method_call_callback,
                            OnExportedCallback exported_callback);

  // Called when the object is exported.
  void OnExported(OnExportedCallback on_exported_callback,
                  const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Helper function for SendSignal().
  void SendSignalInternal(base::TimeTicks start_time,
                          DBusMessage* signal_message);

  // Registers this object to the bus.
  // Returns true on success, or the object is already registered.
  //
  // BLOCKING CALL.
  bool Register();

  // Handles the incoming request messages and dispatches to the exported
  // methods.
  DBusHandlerResult HandleMessage(DBusConnection* connection,
                                  DBusMessage* raw_message);

  // Runs the method. Helper function for HandleMessage().
  void RunMethod(MethodCallCallback method_call_callback,
                 MethodCall* method_call,
                 base::TimeTicks start_time);

  // Callback invoked by service provider to send a response to a method call.
  // Can be called immediately from a MethodCallCallback to implement a
  // synchronous service or called later to implement an asynchronous service.
  void SendResponse(base::TimeTicks start_time,
                    MethodCall* method_call,
                    Response* response);

  // Called on completion of the method run from SendResponse().
  // Takes ownership of |method_call| and |response|.
  void OnMethodCompleted(MethodCall* method_call,
                         Response* response,
                         base::TimeTicks start_time);

  // Called when the object is unregistered.
  void OnUnregistered(DBusConnection* connection);

  // Redirects the function call to HandleMessage().
  static DBusHandlerResult HandleMessageThunk(DBusConnection* connection,
                                              DBusMessage* raw_message,
                                              void* user_data);

  // Redirects the function call to OnUnregistered().
  static void OnUnregisteredThunk(DBusConnection* connection,
                                  void* user_data);

  scoped_refptr<Bus> bus_;
  std::string service_name_;
  std::string object_path_;
  bool object_is_registered_;

  // The method table where keys are absolute method names (i.e. interface
  // name + method name), and values are the corresponding callbacks.
  typedef std::map<std::string, MethodCallCallback> MethodTable;
  MethodTable method_table_;
};

}  // namespace dbus

#endif  // DBUS_EXPORTED_OBJECT_H_
