// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/test_service.h"

#include "base/bind.h"
#include "base/threading/platform_thread.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"

namespace dbus {

const int TestService::kSlowEchoSleepMs = 100;  // In milliseconds.

TestService::TestService()
    : base::Thread("TestService"),
      service_started_(false),
      on_service_started_(&service_started_lock_) {
}

TestService::~TestService() {
}

void TestService::StartService() {
  base::Thread::Options thread_options;
  thread_options.message_loop_type = MessageLoop::TYPE_IO;
  StartWithOptions(thread_options);
}

void TestService::WaitUntilServiceIsStarted() {
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TestService::OnServiceStarted,
                 base::Unretained(this)));
  base::AutoLock auto_lock(service_started_lock_);
  while (!service_started_)
    on_service_started_.Wait();
}

void TestService::OnServiceStarted() {
  base::AutoLock auto_lock(service_started_lock_);
  service_started_ = true;
  on_service_started_.Signal();
}

void TestService::Run(MessageLoop* message_loop) {
  Bus::Options bus_options;
  bus_options.bus_type = Bus::SESSION;
  bus_options.connection_type = Bus::PRIVATE;
  bus_ = new Bus(bus_options);

  exported_object_ = bus_->GetExportedObject(
      "org.chromium.TestService",
      "/org/chromium/TestObject");
  CHECK(exported_object_->ExportMethodAndBlock(
      "org.chromium.TestInterface",
      "Echo",
      base::Bind(&TestService::Echo,
                 base::Unretained(this))));
  CHECK(exported_object_->ExportMethodAndBlock(
      "org.chromium.TestInterface",
      "SlowEcho",
      base::Bind(&TestService::SlowEcho,
                 base::Unretained(this))));
  CHECK(exported_object_->ExportMethodAndBlock(
      "org.chromium.TestInterface",
      "BrokenMethod",
      base::Bind(&TestService::BrokenMethod,
                 base::Unretained(this))));

  message_loop->Run();
}

void TestService::CleanUp() {
  bus_->ShutdownAndBlock();
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
  base::PlatformThread::Sleep(kSlowEchoSleepMs);
  return Echo(method_call);
}

Response* TestService::BrokenMethod(MethodCall* method_call) {
  return NULL;
}

}  // namespace dbus
