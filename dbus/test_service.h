// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_TEST_SERVICE_H_
#define DBUS_TEST_SERVICE_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"

namespace dbus {

class Bus;
class ExportedObject;
class MethodCall;
class Response;

// The test service is used for end-to-end tests.  The service runs in a
// separate thread, so it does not interfere the test code that runs in
// the main thread. Methods such as Echo() and SlowEcho() are exported.
class TestService : public base::Thread {
 public:
  // SlowEcho() sleeps for this period of time before returns.
  static const int kSlowEchoSleepMs;

  TestService();
  virtual ~TestService();

  // Starts the service in a separate thread.
  void StartService();

  // Waits until the service is started (i.e. methods are exported).
  void WaitUntilServiceIsStarted();

 private:
  // Called when the service is started (i.e. the task is run from the
  // message loop).
  void OnServiceStarted();

  // base::Thread override.
  virtual void Run(MessageLoop* message_loop);

  // base::Thread override.
  virtual void CleanUp();

  //
  // Exported methods.
  //

  // Echos the text message received from the method call.
  Response* Echo(MethodCall* method_call);

  // Echos the text message received from the method call, but sleeps for
  // kSlowEchoSleepMs before returning the response.
  Response* SlowEcho(MethodCall* method_call);

  // Returns NULL, instead of a valid Response.
  Response* BrokenMethod(MethodCall* method_call);

  bool service_started_;
  base::Lock service_started_lock_;
  base::ConditionVariable on_service_started_;

  scoped_refptr<Bus> bus_;
  ExportedObject* exported_object_;
};

}  // namespace dbus

#endif  // DBUS_TEST_SERVICE_H_
