// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/blimp_client_context_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/core/contents/blimp_contents_manager.h"
#include "blimp/client/core/session/cross_thread_network_event_observer.h"
#include "blimp/client/public/blimp_client_context_delegate.h"

#if defined(OS_ANDROID)
#include "blimp/client/core/android/blimp_client_context_impl_android.h"
#endif  // OS_ANDROID

namespace blimp {
namespace client {

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/dummy_blimp_client_context.cc should be linked in to
// any binary using BlimpClientContext::Create.
// static
BlimpClientContext* BlimpClientContext::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner) {
#if defined(OS_ANDROID)
  return new BlimpClientContextImplAndroid(io_thread_task_runner);
#else
  return new BlimpClientContextImpl(io_thread_task_runner);
#endif  // defined(OS_ANDROID)
}

BlimpClientContextImpl::BlimpClientContextImpl(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner)
    : BlimpClientContext(),
      io_thread_task_runner_(io_thread_task_runner),
      blimp_contents_manager_(new BlimpContentsManager),
      weak_factory_(this) {
  blimp_connection_statistics_ = new BlimpConnectionStatistics();
  net_components_.reset(new ClientNetworkComponents(
      base::MakeUnique<CrossThreadNetworkEventObserver>(
          weak_factory_.GetWeakPtr(), base::SequencedTaskRunnerHandle::Get()),
      base::WrapUnique(blimp_connection_statistics_)));

  // The |thread_pipe_manager_| must be set up correctly before features are
  // registered.
  thread_pipe_manager_ = base::MakeUnique<ThreadPipeManager>(
      io_thread_task_runner_, net_components_->GetBrowserConnectionHandler());

  // Initialize must only be posted after the calls features have been
  // registered.
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ClientNetworkComponents::Initialize,
                            base::Unretained(net_components_.get())));
}

BlimpClientContextImpl::~BlimpClientContextImpl() {
  io_thread_task_runner_->DeleteSoon(FROM_HERE, net_components_.release());
}

void BlimpClientContextImpl::SetDelegate(BlimpClientContextDelegate* delegate) {
  delegate_ = delegate;
}

std::unique_ptr<BlimpContents> BlimpClientContextImpl::CreateBlimpContents() {
  std::unique_ptr<BlimpContents> blimp_contents =
      blimp_contents_manager_->CreateBlimpContents();
  delegate_->AttachBlimpContentsHelpers(blimp_contents.get());
  return blimp_contents;
}

void BlimpClientContextImpl::OnConnected() {}

void BlimpClientContextImpl::OnDisconnected(int result) {}

}  // namespace client
}  // namespace blimp
