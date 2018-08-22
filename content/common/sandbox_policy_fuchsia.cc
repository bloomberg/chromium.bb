// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_policy_fuchsia.h"

#include <lib/fdio/spawn.h>
#include <stdio.h>
#include <zircon/processargs.h>

#include <fuchsia/fonts/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <memory>
#include <utility>

#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/fuchsia/component_context.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/sandbox/switches.h"

namespace content {
namespace {

constexpr const char* const kRendererServices[] = {
    fuchsia::fonts::FontProvider::Name_};

constexpr const char* const kGpuServices[] = {
    fuchsia::ui::scenic::Scenic::Name_};

base::span<const char* const> GetServicesListForSandboxType(
    service_manager::SandboxType type) {
  switch (type) {
    case service_manager::SANDBOX_TYPE_RENDERER:
      return base::make_span(kRendererServices);
    case service_manager::SANDBOX_TYPE_GPU:
      return base::make_span(kGpuServices);
    default:
      return base::span<const char* const>();
  }
}

}  // namespace

SandboxPolicyFuchsia::SandboxPolicyFuchsia() = default;

SandboxPolicyFuchsia::~SandboxPolicyFuchsia() {
  if (service_directory_) {
    service_directory_task_runner_->DeleteSoon(FROM_HERE,
                                               std::move(service_directory_));
  }
}

void SandboxPolicyFuchsia::Initialize(service_manager::SandboxType type) {
  DCHECK_NE(type, service_manager::SANDBOX_TYPE_INVALID);
  DCHECK_EQ(type_, service_manager::SANDBOX_TYPE_INVALID);

  type_ = type;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          service_manager::switches::kNoSandbox)) {
    type_ = service_manager::SANDBOX_TYPE_NO_SANDBOX;
  }

  // If we need to pass some services for the given sandbox type then create
  // |sandbox_directory_| and initialize it with the corresponding list of
  // services. FilteredServiceDirectory must be initialized on a thread that has
  // async_dispatcher.
  auto services_list = GetServicesListForSandboxType(type_);
  if (!services_list.empty()) {
    service_directory_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    service_directory_ =
        std::make_unique<base::fuchsia::FilteredServiceDirectory>(
            base::fuchsia::ComponentContext::GetDefault());
    for (const char* service_name : services_list)
      service_directory_->AddService(service_name);
  }
}

void SandboxPolicyFuchsia::UpdateLaunchOptionsForSandbox(
    base::LaunchOptions* options) {
  DCHECK_NE(type_, service_manager::SANDBOX_TYPE_INVALID);

  // Always clone stderr to get logs output.
  options->fds_to_remap.push_back(std::make_pair(STDERR_FILENO, STDERR_FILENO));

  if (type_ == service_manager::SANDBOX_TYPE_NO_SANDBOX) {
    options->spawn_flags = FDIO_SPAWN_CLONE_NAMESPACE | FDIO_SPAWN_CLONE_JOB;
    options->clear_environ = false;
    return;
  }

  // Map /pkg (read-only files deployed from the package) into the child's
  // namespace.
  options->paths_to_clone.push_back(base::GetPackageRoot());

  // Clear environmental variables to better isolate the child from
  // this process.
  options->clear_environ = true;

  // Don't clone anything by default.
  options->spawn_flags = 0;

  if (service_directory_) {
    // Provide the child process with a restricted set of services.
    options->paths_to_transfer.push_back(base::PathToTransfer{
        base::FilePath("/svc"), service_directory_->ConnectClient().release()});
  }
}

}  // namespace content
