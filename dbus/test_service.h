// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_TEST_SERVICE_H_
#define DBUS_TEST_SERVICE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/synchronization/waitable_event.h"
#include "dbus/exported_object.h"

namespace base {
class MessageLoopProxy;
}

namespace dbus {

class Bus;
class MethodCall;
class Response;

// The test service is used for end-to-end tests.  The service runs in a
// separate thread, so it does not interfere the test code that runs in
// the main thread.
//
// The test service exports an object with methods such as Echo() and
// SlowEcho(). The object has ability to send "Test" signal.
class TestService : public base::Thread {
 public:
  // Options for the test service.
  struct Options {
    Options();
    ~Options();

    // NULL by default (i.e. don't use the D-Bus thread).
    scoped_refptr<base::MessageLoopProxy> dbus_thread_message_loop_proxy;
  };

  // The number of methods we'll export.
  static const int kNumMethodsToExport;

  TestService(const Options& options);
  virtual ~TestService();

  // Starts the service in a separate thread.
  // Returns true if the thread is started successfully.
  bool StartService();

  // Waits until the service is started (i.e. all methods are exported).
  // Returns true on success.
  bool WaitUntilServiceIsStarted() WARN_UNUSED_RESULT;

  // Shuts down the service and blocks until it's done.
  void ShutdownAndBlock();

  // Returns true if the bus has the D-Bus thread.
  bool HasDBusThread();

  // Sends "Test" signal with the given message from the exported object.
  void SendTestSignal(const std::string& message);

  // Sends "Test" signal with the given message from the root object ("/").
  // This function emulates dbus-send's behavior.
  void SendTestSignalFromRoot(const std::string& message);

 private:
  // Helper function for SendTestSignal().
  void SendTestSignalInternal(const std::string& message);

  // Helper function for SendTestSignalFromRoot.
  void SendTestSignalFromRootInternal(const std::string& message);

  // Helper function for ShutdownAndBlock().
  void ShutdownAndBlockInternal();

  // Called when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // base::Thread override.
  virtual void Run(MessageLoop* message_loop) OVERRIDE;

  //
  // Exported methods.
  //

  // Echos the text message received from the method call.
  void Echo(MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);

  // Echos the text message received from the method call, but sleeps for
  // TestTimeouts::tiny_timeout_ms() before returning the response.
  void SlowEcho(MethodCall* method_call,
                dbus::ExportedObject::ResponseSender response_sender);

  // Echos the text message received from the method call, but sends its
  // response asynchronously after this callback has returned.
  void AsyncEcho(MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender);

  // Returns NULL, instead of a valid Response.
  void BrokenMethod(MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  scoped_refptr<base::MessageLoopProxy> dbus_thread_message_loop_proxy_;
  base::WaitableEvent on_all_methods_exported_;
  // The number of methods actually exported.
  int num_exported_methods_;

  scoped_refptr<Bus> bus_;
  ExportedObject* exported_object_;
};

}  // namespace dbus

#endif  // DBUS_TEST_SERVICE_H_
