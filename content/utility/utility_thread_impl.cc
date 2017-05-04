// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread_impl.h"

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/child/blink_platform_impl.h"
#include "content/child/child_process.h"
#include "content/common/child_process_messages.h"
#include "content/common/utility_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/utility/content_utility_client.h"
#include "content/utility/utility_blink_platform_impl.h"
#include "content/utility/utility_service_factory.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/features/features.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/WebKit/public/web/WebKit.h"

#if defined(OS_POSIX) && BUILDFLAG(ENABLE_PLUGINS)
#include "base/files/file_path.h"
#include "content/common/plugin_list.h"
#endif

namespace content {

UtilityThreadImpl::UtilityThreadImpl()
    : ChildThreadImpl(ChildThreadImpl::Options::Builder()
                          .AutoStartServiceManagerConnection(false)
                          .Build()) {
  Init();
}

UtilityThreadImpl::UtilityThreadImpl(const InProcessChildThreadParams& params)
    : ChildThreadImpl(ChildThreadImpl::Options::Builder()
                          .AutoStartServiceManagerConnection(false)
                          .InBrowserProcess(params)
                          .Build()) {
  Init();
}

UtilityThreadImpl::~UtilityThreadImpl() {
}

void UtilityThreadImpl::Shutdown() {
  ChildThreadImpl::Shutdown();
}

void UtilityThreadImpl::ReleaseProcessIfNeeded() {
  if (batch_mode_)
    return;

  if (IsInBrowserProcess()) {
    // Close the channel to cause UtilityProcessHostImpl to be deleted. We need
    // to take a different code path than the multi-process case because that
    // depends on the child process going away to close the channel, but that
    // can't happen when we're in single process mode.
    channel()->Close();
  } else {
    ChildProcess::current()->ReleaseProcess();
  }
}

void UtilityThreadImpl::EnsureBlinkInitialized() {
  if (blink_platform_impl_ || IsInBrowserProcess()) {
    // We can only initialize WebKit on one thread, and in single process mode
    // we run the utility thread on separate thread. This means that if any code
    // needs WebKit initialized in the utility process, they need to have
    // another path to support single process mode.
    return;
  }

  blink_platform_impl_.reset(new UtilityBlinkPlatformImpl);
  blink::Platform::Initialize(blink_platform_impl_.get());
}

void UtilityThreadImpl::Init() {
  batch_mode_ = false;
  ChildProcess::current()->AddRefProcess();

  auto registry = base::MakeUnique<service_manager::BinderRegistry>();
  registry->AddInterface(
      base::Bind(&UtilityThreadImpl::BindServiceFactoryRequest,
                 base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get());

  content::ServiceManagerConnection* connection = GetServiceManagerConnection();
  if (connection) {
    connection->AddConnectionFilter(
        base::MakeUnique<SimpleConnectionFilter>(std::move(registry)));
  }

  GetContentClient()->utility()->UtilityThreadStarted();

  service_factory_.reset(new UtilityServiceFactory);

  if (connection)
    connection->Start();
}

bool UtilityThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  if (GetContentClient()->utility()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UtilityThreadImpl, msg)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Started, OnBatchModeStarted)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Finished, OnBatchModeFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void UtilityThreadImpl::OnBatchModeStarted() {
  batch_mode_ = true;
}

void UtilityThreadImpl::OnBatchModeFinished() {
  batch_mode_ = false;
  ReleaseProcessIfNeeded();
}

void UtilityThreadImpl::BindServiceFactoryRequest(
    const service_manager::BindSourceInfo& source_info,
    service_manager::mojom::ServiceFactoryRequest request) {
  DCHECK(service_factory_);
  service_factory_bindings_.AddBinding(service_factory_.get(),
                                       std::move(request));
}

}  // namespace content
