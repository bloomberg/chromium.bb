// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread_impl.h"

#include <utility>

#include "content/child/blink_platform_impl.h"
#include "content/child/child_process.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/utility/content_utility_client.h"
#include "content/utility/utility_blink_platform_impl.h"
#include "content/utility/utility_service_factory.h"
#include "ipc/ipc_sync_channel.h"
#include "services/service_manager/public/cpp/binder_registry.h"

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

UtilityThreadImpl::~UtilityThreadImpl() = default;

void UtilityThreadImpl::Shutdown() {
  ChildThreadImpl::Shutdown();
}

void UtilityThreadImpl::ReleaseProcess() {
  if (!IsInBrowserProcess()) {
    ChildProcess::current()->ReleaseProcess();
    return;
  }

  // Close the channel to cause the UtilityProcessHostImpl to be deleted. We
  // need to take a different code path than the multi-process case because
  // that case depends on the child process going away to close the channel,
  // but that can't happen when we're in single process mode.
  channel()->Close();
}

void UtilityThreadImpl::EnsureBlinkInitialized() {
  if (blink_platform_impl_)
    return;

  // We can only initialize Blink on one thread, and in single process mode
  // we run the utility thread on a separate thread. This means that if any
  // code needs Blink initialized in the utility process, they need to have
  // another path to support single process mode.
  if (IsInBrowserProcess())
    return;

  blink_platform_impl_.reset(new UtilityBlinkPlatformImpl);
  blink::Platform::Initialize(blink_platform_impl_.get());
}

void UtilityThreadImpl::Init() {
  ChildProcess::current()->AddRefProcess();

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  registry->AddInterface(
      base::Bind(&UtilityThreadImpl::BindServiceFactoryRequest,
                 base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get());

  content::ServiceManagerConnection* connection = GetServiceManagerConnection();
  if (connection) {
    connection->AddConnectionFilter(
        std::make_unique<SimpleConnectionFilter>(std::move(registry)));
  }

  GetContentClient()->utility()->UtilityThreadStarted();

  service_factory_.reset(new UtilityServiceFactory);

  if (connection)
    connection->Start();
}

bool UtilityThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  return GetContentClient()->utility()->OnMessageReceived(msg);
}

void UtilityThreadImpl::BindServiceFactoryRequest(
    service_manager::mojom::ServiceFactoryRequest request) {
  DCHECK(service_factory_);
  service_factory_bindings_.AddBinding(service_factory_.get(),
                                       std::move(request));
}

}  // namespace content
