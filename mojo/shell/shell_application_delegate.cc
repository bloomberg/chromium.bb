// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_application_delegate.h"

#include <stdint.h>

#include <utility>

#include "base/process/process.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

namespace mojo {
namespace shell {

ShellApplicationDelegate::ShellApplicationDelegate(
    mojo::shell::ApplicationManager* manager)
    : manager_(manager) {}
ShellApplicationDelegate::~ShellApplicationDelegate() {}

void ShellApplicationDelegate::Initialize(ApplicationImpl* app) {}
bool ShellApplicationDelegate::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<mojom::ApplicationManager>(this);
  return true;
}

void ShellApplicationDelegate::Create(
    ApplicationConnection* connection,
    InterfaceRequest<mojom::ApplicationManager> request) {
  bindings_.AddBinding(this, std::move(request));
}

void ShellApplicationDelegate::CreateInstanceForHandle(
    ScopedHandle channel,
    const String& url,
    CapabilityFilterPtr filter,
    InterfaceRequest<mojom::PIDReceiver> pid_receiver) {
  manager_->CreateInstanceForHandle(std::move(channel), GURL(url),
                                    std::move(filter), std::move(pid_receiver));
}

void ShellApplicationDelegate::RegisterProcessWithBroker(
    uint32_t pid, ScopedHandle pipe) {
  // First, for security we want to verify that the given pid's grand parent
  // process is us.
  base::Process process = base::Process::OpenWithExtraPrivileges(
      static_cast<base::ProcessId>(pid));
  if (!process.IsValid()) {
    NOTREACHED();
    return;
  }
  base::ProcessId parent_pid = base::GetParentProcessId(process.Handle());
  base::Process parent_process = base::Process::Open(parent_pid);
  if (!parent_process.IsValid()) {
    NOTREACHED();
    return;
  }
  base::ProcessId grandparent_pid = base::GetParentProcessId(
      parent_process.Handle());
  if (grandparent_pid != base::GetCurrentProcId()) {
#if defined(OS_POSIX)
    // Zygote can also be in between.
    base::ProcessId great_grandparent =
        base::GetParentProcessId(base::Process(grandparent_pid).Handle());
    if (great_grandparent != base::GetCurrentProcId())
#endif
    {
      NOTREACHED();
      return;
    }
  }

  embedder::ScopedPlatformHandle platform_pipe;
  MojoResult rv = embedder::PassWrappedPlatformHandle(
      pipe.release().value(), &platform_pipe);
  CHECK_EQ(rv, MOJO_RESULT_OK);
  embedder::ChildProcessLaunched(process.Handle(), std::move(platform_pipe));
}

void ShellApplicationDelegate::AddListener(
    mojom::ApplicationManagerListenerPtr listener) {
  manager_->AddListener(std::move(listener));
}

}  // namespace shell
}  // namespace mojo
