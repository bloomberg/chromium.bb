// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/context/blimp_client_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/compositor/blob_channel_feature.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/core/contents/blimp_contents_manager.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "blimp/client/core/contents/navigation_feature.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/feedback/blimp_feedback_data.h"
#include "blimp/client/core/geolocation/geolocation_feature.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "blimp/client/core/session/cross_thread_network_event_observer.h"
#include "blimp/client/core/settings/settings_feature.h"
#include "blimp/client/core/switches/blimp_client_switches.h"
#include "blimp/client/public/blimp_client_context_delegate.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/location_arbitrator.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "blimp/client/core/context/android/blimp_client_context_impl_android.h"
#endif  // OS_ANDROID

namespace blimp {
namespace client {

namespace {

const char kDefaultAssignerUrl[] =
    "https://blimp-pa.googleapis.com/v1/assignment";

void DropConnectionOnIOThread(ClientNetworkComponents* net_components) {
  net_components->GetBrowserConnectionHandler()->DropCurrentConnection();
}

}  // namespace

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/dummy_blimp_client_context.cc should be linked in to
// any binary using BlimpClientContext::Create.
// static
BlimpClientContext* BlimpClientContext::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    std::unique_ptr<CompositorDependencies> compositor_dependencies) {
#if defined(OS_ANDROID)
  return new BlimpClientContextImplAndroid(io_thread_task_runner,
                                           file_thread_task_runner,
                                           std::move(compositor_dependencies));
#else
  return new BlimpClientContextImpl(io_thread_task_runner,
                                    file_thread_task_runner,
                                    std::move(compositor_dependencies));
#endif  // defined(OS_ANDROID)
}

BlimpClientContextImpl::BlimpClientContextImpl(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    std::unique_ptr<CompositorDependencies> compositor_dependencies)
    : BlimpClientContext(),
      io_thread_task_runner_(io_thread_task_runner),
      file_thread_task_runner_(file_thread_task_runner),
      blimp_compositor_dependencies_(
          base::MakeUnique<BlimpCompositorDependencies>(
              std::move(compositor_dependencies))),
      blob_channel_feature_(new BlobChannelFeature(this)),
      geolocation_feature_(base::MakeUnique<GeolocationFeature>(
          base::MakeUnique<device::LocationArbitrator>(
              base::MakeUnique<device::GeolocationDelegate>()))),
      ime_feature_(new ImeFeature),
      navigation_feature_(new NavigationFeature),
      render_widget_feature_(new RenderWidgetFeature),
      settings_feature_(new SettingsFeature),
      tab_control_feature_(new TabControlFeature),
      blimp_contents_manager_(
          new BlimpContentsManager(blimp_compositor_dependencies_.get(),
                                   ime_feature_.get(),
                                   navigation_feature_.get(),
                                   render_widget_feature_.get(),
                                   tab_control_feature_.get())),
      weak_factory_(this) {
  net_components_.reset(new ClientNetworkComponents(
      base::MakeUnique<CrossThreadNetworkEventObserver>(
          connection_status_.GetWeakPtr(),
          base::SequencedTaskRunnerHandle::Get())));

  // The |thread_pipe_manager_| must be set up correctly before features are
  // registered.
  thread_pipe_manager_ = base::MakeUnique<ThreadPipeManager>(
      io_thread_task_runner_, net_components_->GetBrowserConnectionHandler());

  RegisterFeatures();
  InitializeSettings();

  // Initialize must only be posted after the features have been
  // registered.
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ClientNetworkComponents::Initialize,
                            base::Unretained(net_components_.get())));

  UMA_HISTOGRAM_BOOLEAN("Blimp.Supported", true);
}

BlimpClientContextImpl::~BlimpClientContextImpl() {
  io_thread_task_runner_->DeleteSoon(FROM_HERE, net_components_.release());
}

void BlimpClientContextImpl::SetDelegate(BlimpClientContextDelegate* delegate) {
  delegate_ = delegate;
}

std::unique_ptr<BlimpContents> BlimpClientContextImpl::CreateBlimpContents(
    gfx::NativeWindow window) {
  std::unique_ptr<BlimpContents> blimp_contents =
      blimp_contents_manager_->CreateBlimpContents(window);
  if (blimp_contents)
    delegate_->AttachBlimpContentsHelpers(blimp_contents.get());
  return blimp_contents;
}

void BlimpClientContextImpl::Connect() {
  // Start Blimp authentication flow. The OAuth2 token will be used in
  // assignment source.
  GetIdentitySource()->Connect();
}

void BlimpClientContextImpl::ConnectWithAssignment(
    const Assignment& assignment) {
  io_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ClientNetworkComponents::ConnectWithAssignment,
                 base::Unretained(net_components_.get()), assignment));
}

std::unordered_map<std::string, std::string>
BlimpClientContextImpl::CreateFeedbackData() {
  return CreateBlimpFeedbackData(blimp_contents_manager_.get());
}

void BlimpClientContextImpl::OnAuthTokenReceived(
    const std::string& client_auth_token) {
  if (!assignment_source_) {
    assignment_source_.reset(new AssignmentSource(
        GetAssignerURL(), io_thread_task_runner_, file_thread_task_runner_));
  }

  VLOG(1) << "Trying to get assignment.";
  assignment_source_->GetAssignment(
      client_auth_token,
      base::Bind(&BlimpClientContextImpl::OnAssignmentReceived,
                 weak_factory_.GetWeakPtr()));
}

GURL BlimpClientContextImpl::GetAssignerURL() {
  return GURL(kDefaultAssignerUrl);
}

IdentitySource* BlimpClientContextImpl::GetIdentitySource() {
  if (!identity_source_) {
    CreateIdentitySource();
  }
  return identity_source_.get();
}

ConnectionStatus* BlimpClientContextImpl::GetConnectionStatus() {
  return &connection_status_;
}

void BlimpClientContextImpl::OnAssignmentReceived(
    AssignmentRequestResult result,
    const Assignment& assignment) {
  VLOG(1) << "Assignment result: " << result;

  // Cache engine info.
  connection_status_.OnAssignmentResult(result, assignment);

  // Inform the embedder of the assignment result.
  if (delegate_) {
    delegate_->OnAssignmentConnectionAttempted(result, assignment);
  }

  if (result != ASSIGNMENT_REQUEST_RESULT_OK) {
    LOG(ERROR) << "Assignment failed, reason: " << result;
    return;
  }

  ConnectWithAssignment(assignment);
}

void BlimpClientContextImpl::RegisterFeatures() {
  // Register features' message senders and receivers.
  thread_pipe_manager_->RegisterFeature(BlimpMessage::kBlobChannel,
                                        blob_channel_feature_.get());
  geolocation_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kGeolocation,
                                            geolocation_feature_.get()));
  ime_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kIme,
                                            ime_feature_.get()));
  navigation_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kNavigation,
                                            navigation_feature_.get()));
  render_widget_feature_->set_outgoing_input_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kInput,
                                            render_widget_feature_.get()));
  render_widget_feature_->set_outgoing_compositor_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kCompositor,
                                            render_widget_feature_.get()));
  thread_pipe_manager_->RegisterFeature(BlimpMessage::kRenderWidget,
                                        render_widget_feature_.get());
  settings_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kSettings,
                                            settings_feature_.get()));
  tab_control_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kTabControl,
                                            tab_control_feature_.get()));
}

void BlimpClientContextImpl::InitializeSettings() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDownloadWholeDocument))
    settings_feature_->SetRecordWholeDocument(true);
}

void BlimpClientContextImpl::DropConnection() {
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DropConnectionOnIOThread, net_components_.get()));
}

void BlimpClientContextImpl::CreateIdentitySource() {
  identity_source_ = base::MakeUnique<IdentitySource>(
      delegate_, base::Bind(&BlimpClientContextImpl::OnAuthTokenReceived,
                            base::Unretained(this)));
}

void BlimpClientContextImpl::OnImageDecodeError() {
  // Currently we just drop the connection on image decoding error.
  DropConnection();
}

}  // namespace client
}  // namespace blimp
