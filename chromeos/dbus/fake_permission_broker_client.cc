// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_permission_broker_client.h"

#include <fcntl.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

namespace {

const char kOpenFailedError[] = "open_failed";

// So that real devices can be accessed by tests and "Chromium OS on Linux" this
// function implements a simplified version of the method implemented by the
// permission broker by opening the path specified and returning the resulting
// file descriptor.
void OpenPath(const std::string& path,
              const PermissionBrokerClient::OpenPathCallback& callback,
              const PermissionBrokerClient::ErrorCallback& error_callback,
              scoped_refptr<base::TaskRunner> task_runner) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.c_str(), O_RDWR)));
  if (!fd.is_valid()) {
    int error_code = logging::GetLastSystemErrorCode();
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(error_callback, kOpenFailedError,
                   base::StringPrintf(
                       "Failed to open '%s': %s", path.c_str(),
                       logging::SystemErrorCodeToString(error_code).c_str())));
    return;
  }

  task_runner->PostTask(FROM_HERE, base::Bind(callback, base::Passed(&fd)));
}

}  // namespace

FakePermissionBrokerClient::FakePermissionBrokerClient() {}

FakePermissionBrokerClient::~FakePermissionBrokerClient() {}

void FakePermissionBrokerClient::Init(dbus::Bus* bus) {}

void FakePermissionBrokerClient::CheckPathAccess(
    const std::string& path,
    const ResultCallback& callback) {
  callback.Run(true);
}

void FakePermissionBrokerClient::OpenPath(const std::string& path,
                                          const OpenPathCallback& callback,
                                          const ErrorCallback& error_callback) {
  base::PostTaskWithTraits(
      FROM_HERE, base::TaskTraits()
                     .WithShutdownBehavior(
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                     .MayBlock(),
      base::Bind(&chromeos::OpenPath, path, callback, error_callback,
                 base::ThreadTaskRunnerHandle::Get()));
}

void FakePermissionBrokerClient::RequestTcpPortAccess(
    uint16_t port,
    const std::string& interface,
    int lifeline_fd,
    const ResultCallback& callback) {
  callback.Run(true);
}

void FakePermissionBrokerClient::RequestUdpPortAccess(
    uint16_t port,
    const std::string& interface,
    int lifeline_fd,
    const ResultCallback& callback) {
  callback.Run(true);
}

void FakePermissionBrokerClient::ReleaseTcpPort(
    uint16_t port,
    const std::string& interface,
    const ResultCallback& callback) {
  callback.Run(true);
}

void FakePermissionBrokerClient::ReleaseUdpPort(
    uint16_t port,
    const std::string& interface,
    const ResultCallback& callback) {
  callback.Run(true);
}

}  // namespace chromeos
