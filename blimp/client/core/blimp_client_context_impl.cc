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
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/session/cross_thread_network_event_observer.h"
#include "blimp/client/public/blimp_client_context_delegate.h"

#if defined(OS_ANDROID)
#include "blimp/client/core/android/blimp_client_context_impl_android.h"
#endif  // OS_ANDROID

namespace blimp {
namespace client {

namespace {
const char kDefaultAssignerUrl[] =
    "https://blimp-pa.googleapis.com/v1/assignment";
}  // namespace

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/dummy_blimp_client_context.cc should be linked in to
// any binary using BlimpClientContext::Create.
// static
BlimpClientContext* BlimpClientContext::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner) {
#if defined(OS_ANDROID)
  return new BlimpClientContextImplAndroid(io_thread_task_runner,
                                           file_thread_task_runner);
#else
  return new BlimpClientContextImpl(io_thread_task_runner,
                                    file_thread_task_runner);
#endif  // defined(OS_ANDROID)
}

BlimpClientContextImpl::BlimpClientContextImpl(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner)
    : BlimpClientContext(),
      io_thread_task_runner_(io_thread_task_runner),
      file_thread_task_runner_(file_thread_task_runner),
      tab_control_feature_(new TabControlFeature),
      blimp_contents_manager_(new BlimpContentsManager(
          tab_control_feature_.get())),
      weak_factory_(this) {
  net_components_.reset(new ClientNetworkComponents(
      base::MakeUnique<CrossThreadNetworkEventObserver>(
          weak_factory_.GetWeakPtr(), base::SequencedTaskRunnerHandle::Get())));

  // The |thread_pipe_manager_| must be set up correctly before features are
  // registered.
  thread_pipe_manager_ = base::MakeUnique<ThreadPipeManager>(
      io_thread_task_runner_, net_components_->GetBrowserConnectionHandler());

  RegisterFeatures();

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

void BlimpClientContextImpl::Connect() {
  // Lazy initialization of IdentitySource.
  if (!identity_source_) {
    identity_source_ = base::MakeUnique<IdentitySource>(
        delegate_,
        base::Bind(&BlimpClientContextImpl::ConnectToAssignmentSource,
                   base::Unretained(this)));
  }

  // Start Blimp authentication flow. The OAuth2 token will be used in
  // assignment source.
  identity_source_->Connect();
}

void BlimpClientContextImpl::ConnectToAssignmentSource(
    const std::string& client_auth_token) {
  if (!assignment_source_) {
    assignment_source_.reset(new AssignmentSource(
        GetAssignerURL(), io_thread_task_runner_, file_thread_task_runner_));
  }

  VLOG(1) << "Trying to get assignment.";
  assignment_source_->GetAssignment(
      client_auth_token,
      base::Bind(&BlimpClientContextImpl::ConnectWithAssignment,
                 weak_factory_.GetWeakPtr()));
}

void BlimpClientContextImpl::OnConnected() {}

void BlimpClientContextImpl::OnDisconnected(int result) {}

TabControlFeature* BlimpClientContextImpl::GetTabControlFeature() const {
  return tab_control_feature_.get();
}

GURL BlimpClientContextImpl::GetAssignerURL() {
  return GURL(kDefaultAssignerUrl);
}

void BlimpClientContextImpl::ConnectWithAssignment(
    AssignmentRequestResult result,
    const Assignment& assignment) {
  VLOG(1) << "Assignment result: " << result;

  if (delegate_) {
    delegate_->OnAssignmentConnectionAttempted(result, assignment);
  }

  if (result != ASSIGNMENT_REQUEST_RESULT_OK) {
    LOG(ERROR) << "Assignment failed, reason: " << result;
    return;
  }

  io_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ClientNetworkComponents::ConnectWithAssignment,
                 base::Unretained(net_components_.get()), assignment));
}

void BlimpClientContextImpl::RegisterFeatures() {
  // Register features' message senders and receivers.
  tab_control_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kTabControl,
                                            tab_control_feature_.get()));
}

}  // namespace client
}  // namespace blimp
