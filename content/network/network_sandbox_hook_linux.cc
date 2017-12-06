// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/network_sandbox_hook_linux.h"
#include "sandbox/linux/syscall_broker/broker_command.h"

#include "base/rand_util.h"
#include "base/sys_info.h"

using sandbox::syscall_broker::BrokerFilePermission;

namespace content {

bool NetworkPreSandboxHook(service_manager::SandboxLinux::Options options) {
  sandbox::syscall_broker::BrokerCommandSet command_set;
  command_set.set(sandbox::syscall_broker::COMMAND_ACCESS);
  command_set.set(sandbox::syscall_broker::COMMAND_OPEN);
  command_set.set(sandbox::syscall_broker::COMMAND_READLINK);
  command_set.set(sandbox::syscall_broker::COMMAND_RENAME);
  command_set.set(sandbox::syscall_broker::COMMAND_STAT);

  // TODO(tsepez): FIX THIS.
  std::vector<BrokerFilePermission> file_permissions = {
      BrokerFilePermission::ReadWriteCreateRecursive("/")};

  service_manager::SandboxLinux::GetInstance()->StartBrokerProcess(
      command_set, std::move(file_permissions),
      service_manager::SandboxLinux::PreSandboxHook(), options);

  return true;
}

}  // namespace content
