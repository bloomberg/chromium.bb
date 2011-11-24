// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/test_service.h"

#include "base/bind.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"

namespace dbus {

// Echo, SlowEcho, BrokenMethod.
const int TestService::kNumMethodsToExport = 3;

TestService::Options::Options() {
}

TestService::Options::~Options() {
}

TestService::TestService(const Options& options)
    : base::Thread("TestService"),
      dbus_thread_message_loop_proxy_(options.dbus_thread_message_loop_proxy),
      on_all_methods_exported_(false, false),
      num_exported_methods_(0) {
}

TestService::~TestService() {
  Stop();
}

bool TestService::StartService() {
  base::Thread::Options thread_options;
  thread_options.message_loop_type = MessageLoop::TYPE_IO;
  return StartWithOptions(thread_options);
}

bool TestService::WaitUntilServiceIsStarted() {
  const base::TimeDelta timeout(
      base::TimeDelta::FromMilliseconds(
          TestTimeouts::action_max_timeout_ms()));
  // Wait until all methods are exported.
  return on_all_methods_exported_.TimedWait(timeout);
}

void TestService::ShutdownAndBlock() {
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TestService::ShutdownAndBlockInternal,
                 base::Unretained(this)));
}

bool TestService::HasDBusThread() {
  return bus_->HasDBusThread();
}

void TestService::ShutdownAndBlockInternal() {
  if (HasDBusThread())
    bus_->ShutdownOnDBusThreadAndBlock();
  else
    bus_->ShutdownAndBlock();
}

void TestService::SendTestSignal(const std::string& message) {
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TestService::SendTestSignalInternal,
                 base::Unretained(this),
                 message));
}

void TestService::SendTestSignalFromRoot(const std::string& message) {
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TestService::SendTestSignalFromRootInternal,
                 base::Unretained(this),
                 message));
}

void TestService::SendTestSignalInternal(const std::string& message) {
  dbus::Signal signal("org.chromium.TestInterface", "Test");
  dbus::MessageWriter writer(&signal);
  writer.AppendString(message);
  exported_object_->SendSignal(&signal);
}

void TestService::SendTestSignalFromRootInternal(const std::string& message) {
  dbus::Signal signal("org.chromium.TestInterface", "Test");
  dbus::MessageWriter writer(&signal);
  writer.AppendString(message);

  // Use "/" just like dbus-send does.
  ExportedObject* root_object =
      bus_->GetExportedObject("org.chromium.TestService",
                              "/");
  root_object->SendSignal(&signal);
}

void TestService::OnExported(const std::string& interface_name,
                             const std::string& method_name,
                             bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export: " << interface_name << "."
               << method_name;
    // Returning here will make WaitUntilServiceIsStarted() to time out
    // and return false.
    return;
  }

  ++num_exported_methods_;
  if (num_exported_methods_ == kNumMethodsToExport)
    on_all_methods_exported_.Signal();
}

void TestService::Run(MessageLoop* message_loop) {
  Bus::Options bus_options;
  bus_options.bus_type = Bus::SESSION;
  bus_options.connection_type = Bus::PRIVATE;
  bus_options.dbus_thread_message_loop_proxy = dbus_thread_message_loop_proxy_;
  bus_ = new Bus(bus_options);

  exported_object_ = bus_->GetExportedObject(
      "org.chromium.TestService",
      "/org/chromium/TestObject");

  int num_methods = 0;
  exported_object_->ExportMethod(
      "org.chromium.TestInterface",
      "Echo",
      base::Bind(&TestService::Echo,
                 base::Unretained(this)),
      base::Bind(&TestService::OnExported,
                 base::Unretained(this)));
  ++num_methods;

  exported_object_->ExportMethod(
      "org.chromium.TestInterface",
      "SlowEcho",
      base::Bind(&TestService::SlowEcho,
                 base::Unretained(this)),
      base::Bind(&TestService::OnExported,
                 base::Unretained(this)));
  ++num_methods;

  exported_object_->ExportMethod(
      "org.chromium.TestInterface",
      "BrokenMethod",
      base::Bind(&TestService::BrokenMethod,
                 base::Unretained(this)),
      base::Bind(&TestService::OnExported,
                 base::Unretained(this)));
  ++num_methods;

  // Just print an error message as we don't want to crash tests.
  // Tests will fail at a call to WaitUntilServiceIsStarted().
  if (num_methods != kNumMethodsToExport) {
    LOG(ERROR) << "The number of methods does not match";
  }
  message_loop->Run();
}

Response* TestService::Echo(MethodCall* method_call) {
  MessageReader reader(method_call);
  std::string text_message;
  if (!reader.PopString(&text_message))
    return NULL;

  Response* response = Response::FromMethodCall(method_call);
  MessageWriter writer(response);
  writer.AppendString(text_message);
  return response;
}

Response* TestService::SlowEcho(MethodCall* method_call) {
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout_ms());
  return Echo(method_call);
}

Response* TestService::BrokenMethod(MethodCall* method_call) {
  return NULL;
}

}  // namespace dbus
