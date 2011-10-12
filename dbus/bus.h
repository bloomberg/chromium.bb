// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_BUS_H_
#define DBUS_BUS_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <dbus/dbus.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/tracked_objects.h"

class MessageLoop;

namespace base {
class Thread;
class MessageLoopProxy;
}

namespace dbus {

class ExportedObject;
class ObjectProxy;

// Bus is used to establish a connection with D-Bus, create object
// proxies, and export objects.
//
// For asynchronous operations such as an asynchronous method call, the
// bus object will use a message loop to monitor the underlying file
// descriptor used for D-Bus communication. By default, the bus will use
// the current thread's MessageLoopForIO. If |dbus_thread_message_loop_proxy|
// option is specified, the bus will use that message loop instead.
//
// THREADING
//
// In the D-Bus library, we use the two threads:
//
// - The origin thread: the thread that created the Bus object.
// - The D-Bus thread: the thread servicing |dbus_thread_message_loop_proxy|.
//
// The origin thread is usually Chrome's UI thread. The D-Bus thread is
// usually a dedicated thread for the D-Bus library.
//
// BLOCKING CALLS
//
// Functions that issue blocking calls are marked "BLOCKING CALL" and
// these functions should be called in the D-Bus thread (if
// supplied). AssertOnDBusThread() is placed in these functions.
//
// Note that it's hard to tell if a libdbus function is actually blocking
// or not (ex. dbus_bus_request_name() internally calls
// dbus_connection_send_with_reply_and_block(), which is a blocking
// call). To err on the safe side, we consider all libdbus functions that
// deal with the connection to dbus-daemon to be blocking.
//
// SHUTDOWN
//
// The Bus object must be shut down manually by ShutdownAndBlock() and
// friends. We require the manual shutdown to make the operation explicit
// rather than doing it silently in the destructor.
//
// EXAMPLE USAGE:
//
// Synchronous method call:
//
//   dbus::Bus::Options options;
//   // Set up the bus options here.
//   ...
//   dbus::Bus bus(options);
//
//   dbus::ObjectProxy* object_proxy =
//       bus.GetObjectProxy(service_name, object_path);
//
//   dbus::MethodCall method_call(interface_name, method_name);
//   scoped_ptr<dbus::Response> response(
//       object_proxy.CallMethodAndBlock(&method_call, timeout_ms));
//   if (response.get() != NULL) {  // Success.
//     ...
//   }
//
// Asynchronous method call:
//
//   void OnResponse(dbus::Response* response) {
//     // response is NULL if the method call failed.
//     if (!response)
//       return;
//   }
//
//   ...
//   object_proxy.CallMethod(&method_call, timeout_ms,
//                           base::Bind(&OnResponse));
//
// Exporting a method:
//
//   Response* Echo(dbus::MethodCall* method_call) {
//     // Do something with method_call.
//     Response* response = Response::FromMethodCall(method_call);
//     // Build response here.
//     return response;
//   }
//
//   void OnExported(const std::string& interface_name,
//                   const std::string& object_path,
//                   bool success) {
//     // success is true if the method was exported successfully.
//   }
//
//   ...
//   dbus::ExportedObject* exported_object =
//       bus.GetExportedObject(service_name, object_path);
//   exported_object.ExportMethod(interface_name, method_name,
//                                base::Bind(&Echo),
//                                base::Bind(&OnExported));
//
// WHY IS THIS A REF COUNTED OBJECT?
//
// Bus is a ref counted object, to ensure that |this| of the object is
// alive when callbacks referencing |this| are called. However, after the
// bus is shut down, |connection_| can be NULL. Hence, callbacks should
// not rely on that |connection_| is alive.
class Bus : public base::RefCountedThreadSafe<Bus> {
 public:
  // Specifies the bus type. SESSION is used to communicate with per-user
  // services like GNOME applications. SYSTEM is used to communicate with
  // system-wide services like NetworkManager.
  enum BusType {
    SESSION = DBUS_BUS_SESSION,
    SYSTEM  = DBUS_BUS_SYSTEM,
  };

  // Specifies the connection type. PRIVATE should usually be used unless
  // you are sure that SHARED is safe for you, which is unlikely the case
  // in Chrome.
  //
  // PRIVATE gives you a private connection, that won't be shared with
  // other Bus objects.
  //
  // SHARED gives you a connection shared among other Bus objects, which
  // is unsafe if the connection is shared with multiple threads.
  enum ConnectionType {
    PRIVATE,
    SHARED,
  };

  // Options used to create a Bus object.
  struct Options {
    Options();
    ~Options();

    BusType bus_type;  // SESSION by default.
    ConnectionType connection_type;  // PRIVATE by default.
    // If dbus_thread_message_loop_proxy is set, the bus object will use that
    // message loop to process asynchronous operations.
    //
    // The thread servicing the message loop proxy should meet the following
    // requirements:
    // 1) Already running.
    // 2) Has a MessageLoopForIO.
    scoped_refptr<base::MessageLoopProxy> dbus_thread_message_loop_proxy;
  };

  // Creates a Bus object. The actual connection will be established when
  // Connect() is called.
  explicit Bus(const Options& options);

  // Gets the object proxy for the given service name and the object path.
  // The caller must not delete the returned object.
  //
  // Returns an existing object proxy if the bus object already owns the
  // object proxy for the given service name and the object path.
  // Never returns NULL.
  //
  // The bus will own all object proxies created by the bus, to ensure
  // that the object proxies are detached from remote objects at the
  // shutdown time of the bus.
  //
  // The object proxy is used to call methods of remote objects, and
  // receive signals from them.
  //
  // |service_name| looks like "org.freedesktop.NetworkManager", and
  // |object_path| looks like "/org/freedesktop/NetworkManager/Devices/0".
  //
  // Must be called in the origin thread.
  virtual ObjectProxy* GetObjectProxy(const std::string& service_name,
                                      const std::string& object_path);

  // Gets the exported object for the given service name and the object
  // path. The caller must not delete the returned object.
  //
  // Returns an existing exported object if the bus object already owns
  // the exported object for the given service name and the object path.
  // Never returns NULL.
  //
  // The bus will own all exported objects created by the bus, to ensure
  // that the exported objects are unregistered at the shutdown time of
  // the bus.
  //
  // The exported object is used to export methods of local objects, and
  // send signal from them.
  //
  // Must be called in the origin thread.
  virtual ExportedObject* GetExportedObject(const std::string& service_name,
                                            const std::string& object_path);

  // Shuts down the bus and blocks until it's done. More specifically, this
  // function does the following:
  //
  // - Unregisters the object paths
  // - Releases the service names
  // - Closes the connection to dbus-daemon.
  //
  // BLOCKING CALL.
  virtual void ShutdownAndBlock();

  // Similar to ShutdownAndBlock(), but this function is used to
  // synchronously shut down the bus that uses the D-Bus thread. This
  // function is intended to be used at the very end of the browser
  // shutdown, where it makes more sense to shut down the bus
  // synchronously, than trying to make it asynchronous.
  //
  // BLOCKING CALL, but must be called in the origin thread.
  virtual void ShutdownOnDBusThreadAndBlock();

  // Returns true if the shutdown has been completed.
  bool shutdown_completed() { return shutdown_completed_; }

  //
  // The public functions below are not intended to be used in client
  // code. These are used to implement ObjectProxy and ExportedObject.
  //

  // Connects the bus to the dbus-daemon.
  // Returns true on success, or the bus is already connected.
  //
  // BLOCKING CALL.
  virtual bool Connect();

  // Requests the ownership of the given service name.
  // Returns true on success, or the the service name is already obtained.
  //
  // BLOCKING CALL.
  virtual bool RequestOwnership(const std::string& service_name);

  // Releases the ownership of the given service name.
  // Returns true on success.
  //
  // BLOCKING CALL.
  virtual bool ReleaseOwnership(const std::string& service_name);

  // Sets up async operations.
  // Returns true on success, or it's already set up.
  // This function needs to be called before starting async operations.
  //
  // BLOCKING CALL.
  virtual bool SetUpAsyncOperations();

  // Sends a message to the bus and blocks until the response is
  // received. Used to implement synchronous method calls.
  //
  // BLOCKING CALL.
  virtual DBusMessage* SendWithReplyAndBlock(DBusMessage* request,
                                             int timeout_ms,
                                             DBusError* error);

  // Requests to send a message to the bus. The reply is handled with
  // |pending_call| at a later time.
  //
  // BLOCKING CALL.
  virtual void SendWithReply(DBusMessage* request,
                             DBusPendingCall** pending_call,
                             int timeout_ms);

  // Requests to send a message to the bus. The message serial number will
  // be stored in |serial|.
  //
  // BLOCKING CALL.
  virtual void Send(DBusMessage* request, uint32* serial);

  // Adds the message filter function. |filter_function| will be called
  // when incoming messages are received. Returns true on success.
  //
  // When a new incoming message arrives, filter functions are called in
  // the order that they were added until the the incoming message is
  // handled by a filter function.
  //
  // The same filter function associated with the same user data cannot be
  // added more than once. Returns false for this case.
  //
  // BLOCKING CALL.
  virtual bool AddFilterFunction(DBusHandleMessageFunction filter_function,
                                 void* user_data);

  // Removes the message filter previously added by AddFilterFunction().
  // Returns true on success.
  //
  // BLOCKING CALL.
  virtual bool RemoveFilterFunction(DBusHandleMessageFunction filter_function,
                                    void* user_data);

  // Adds the match rule. Messages that match the rule will be processed
  // by the filter functions added by AddFilterFunction().
  //
  // You cannot specify which filter function to use for a match rule.
  // Instead, you should check if an incoming message is what you are
  // interested in, in the filter functions.
  //
  // The same match rule can be added more than once, but ignored from the
  // second time.
  //
  // The match rule looks like:
  // "type='signal', interface='org.chromium.SomeInterface'".
  //
  // See "Message Bus Message Routing" section in the D-Bus specification
  // for details about match rules:
  // http://dbus.freedesktop.org/doc/dbus-specification.html#message-bus-routing
  //
  // BLOCKING CALL.
  virtual void AddMatch(const std::string& match_rule, DBusError* error);

  // Removes the match rule previously added by AddMatch().
  //
  // BLOCKING CALL.
  virtual void RemoveMatch(const std::string& match_rule, DBusError* error);

  // Tries to register the object path. Returns true on success.
  // Returns false if the object path is already registered.
  //
  // |message_function| in |vtable| will be called every time when a new
  // |message sent to the object path arrives.
  //
  // The same object path must not be added more than once.
  //
  // See also documentation of |dbus_connection_try_register_object_path| at
  // http://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html
  //
  // BLOCKING CALL.
  virtual bool TryRegisterObjectPath(const std::string& object_path,
                                     const DBusObjectPathVTable* vtable,
                                     void* user_data,
                                     DBusError* error);

  // Unregister the object path.
  //
  // BLOCKING CALL.
  virtual void UnregisterObjectPath(const std::string& object_path);

  // Posts the task to the message loop of the thread that created the bus.
  virtual void PostTaskToOriginThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task);

  // Posts the task to the message loop of the D-Bus thread. If D-Bus
  // thread is not supplied, the message loop of the origin thread will be
  // used.
  virtual void PostTaskToDBusThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task);

  // Posts the delayed task to the message loop of the D-Bus thread. If
  // D-Bus thread is not supplied, the message loop of the origin thread
  // will be used.
  virtual void PostDelayedTaskToDBusThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      int delay_ms);

  // Returns true if the bus has the D-Bus thread.
  virtual bool HasDBusThread();

  // Check whether the current thread is on the origin thread (the thread
  // that created the bus). If not, DCHECK will fail.
  virtual void AssertOnOriginThread();

  // Check whether the current thread is on the D-Bus thread. If not,
  // DCHECK will fail. If the D-Bus thread is not supplied, it calls
  // AssertOnOriginThread().
  virtual void AssertOnDBusThread();

  // Returns true if the bus is connected to D-Bus.
  bool is_connected() { return connection_ != NULL; }

 protected:
  // This is protected, so we can define sub classes.
  virtual ~Bus();

 private:
  friend class base::RefCountedThreadSafe<Bus>;

  // Helper function used for ShutdownOnDBusThreadAndBlock().
  void ShutdownOnDBusThreadAndBlockInternal();

  // Processes the all incoming data to the connection, if any.
  //
  // BLOCKING CALL.
  void ProcessAllIncomingDataIfAny();

  // Called when a watch object is added. Used to start monitoring the
  // file descriptor used for D-Bus communication.
  dbus_bool_t OnAddWatch(DBusWatch* raw_watch);

  // Called when a watch object is removed.
  void OnRemoveWatch(DBusWatch* raw_watch);

  // Called when the "enabled" status of |raw_watch| is toggled.
  void OnToggleWatch(DBusWatch* raw_watch);

  // Called when a timeout object is added. Used to start monitoring
  // timeout for method calls.
  dbus_bool_t OnAddTimeout(DBusTimeout* raw_timeout);

  // Called when a timeout object is removed.
  void OnRemoveTimeout(DBusTimeout* raw_timeout);

  // Called when the "enabled" status of |raw_timeout| is toggled.
  void OnToggleTimeout(DBusTimeout* raw_timeout);

  // Called when the dispatch status (i.e. if any incoming data is
  // available) is changed.
  void OnDispatchStatusChanged(DBusConnection* connection,
                               DBusDispatchStatus status);

  // Callback helper functions. Redirects to the corresponding member function.
  static dbus_bool_t OnAddWatchThunk(DBusWatch* raw_watch, void* data);
  static void OnRemoveWatchThunk(DBusWatch* raw_watch, void* data);
  static void OnToggleWatchThunk(DBusWatch* raw_watch, void* data);
  static dbus_bool_t OnAddTimeoutThunk(DBusTimeout* raw_timeout, void* data);
  static void OnRemoveTimeoutThunk(DBusTimeout* raw_timeout, void* data);
  static void OnToggleTimeoutThunk(DBusTimeout* raw_timeout, void* data);
  static void OnDispatchStatusChangedThunk(DBusConnection* connection,
                                           DBusDispatchStatus status,
                                           void* data);
  const BusType bus_type_;
  const ConnectionType connection_type_;
  scoped_refptr<base::MessageLoopProxy> dbus_thread_message_loop_proxy_;
  base::WaitableEvent on_shutdown_;
  DBusConnection* connection_;

  scoped_refptr<base::MessageLoopProxy> origin_message_loop_proxy_;
  base::PlatformThreadId origin_thread_id_;

  std::set<std::string> owned_service_names_;
  // The following sets are used to check if rules/object_paths/filters
  // are properly cleaned up before destruction of the bus object.
  std::set<std::string> match_rules_added_;
  std::set<std::string> registered_object_paths_;
  std::set<std::pair<DBusHandleMessageFunction, void*> >
      filter_functions_added_;

  // ObjectProxyTable is used to hold the object proxies created by the
  // bus object. Key is a concatenated string of service name + object path,
  // like "org.chromium.TestService/org/chromium/TestObject".
  typedef std::map<std::string,
                   scoped_refptr<dbus::ObjectProxy> > ObjectProxyTable;
  ObjectProxyTable object_proxy_table_;

  // ExportedObjectTable is used to hold the exported objects created by
  // the bus object. Key is a concatenated string of service name +
  // object path, like "org.chromium.TestService/org/chromium/TestObject".
  typedef std::map<std::string,
                   scoped_refptr<dbus::ExportedObject> > ExportedObjectTable;
  ExportedObjectTable exported_object_table_;

  bool async_operations_set_up_;
  bool shutdown_completed_;

  // Counters to make sure that OnAddWatch()/OnRemoveWatch() and
  // OnAddTimeout()/OnRemoveTimeou() are balanced.
  int num_pending_watches_;
  int num_pending_timeouts_;

  DISALLOW_COPY_AND_ASSIGN(Bus);
};

}  // namespace dbus

#endif  // DBUS_BUS_H_
