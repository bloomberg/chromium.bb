// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_service_impl.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/presentation_connection_message.h"

namespace content {

namespace {

const int kInvalidRequestId = -1;

int GetNextRequestId() {
  static int next_request_id = 0;
  return ++next_request_id;
}

void InvokeNewPresentationCallbackWithError(
    PresentationServiceImpl::NewPresentationCallback callback) {
  std::move(callback).Run(
      base::nullopt,
      PresentationError(
          PRESENTATION_ERROR_PREVIOUS_START_IN_PROGRESS,
          "There is already an unsettled Promise from a previous call "
          "to start."));
}

}  // namespace

PresentationServiceImpl::PresentationServiceImpl(
    RenderFrameHost* render_frame_host,
    WebContents* web_contents,
    ControllerPresentationServiceDelegate* controller_delegate,
    ReceiverPresentationServiceDelegate* receiver_delegate)
    : WebContentsObserver(web_contents),
      controller_delegate_(controller_delegate),
      receiver_delegate_(receiver_delegate),
      start_presentation_request_id_(kInvalidRequestId),
      weak_factory_(this) {
  DCHECK(render_frame_host);
  DCHECK(web_contents);
  CHECK(render_frame_host->IsRenderFrameLive());

  render_process_id_ = render_frame_host->GetProcess()->GetID();
  render_frame_id_ = render_frame_host->GetRoutingID();
  DVLOG(2) << "PresentationServiceImpl: "
           << render_process_id_ << ", " << render_frame_id_;

  if (auto* delegate = GetPresentationServiceDelegate())
    delegate->AddObserver(render_process_id_, render_frame_id_, this);
}

PresentationServiceImpl::~PresentationServiceImpl() {
  DVLOG(2) << __FUNCTION__ << ": " << render_process_id_ << ", "
           << render_frame_id_;

  if (auto* delegate = GetPresentationServiceDelegate())
    delegate->RemoveObserver(render_process_id_, render_frame_id_);
}

// static
void PresentationServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    const service_manager::BindSourceInfo& source_info,
    blink::mojom::PresentationServiceRequest request) {
  DVLOG(2) << "CreateMojoService";
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  auto* browser = GetContentClient()->browser();
  auto* receiver_delegate =
      browser->GetReceiverPresentationServiceDelegate(web_contents);

  // In current implementation, web_contents can be controller or receiver
  // but not both.
  auto* controller_delegate =
      receiver_delegate
          ? nullptr
          : browser->GetControllerPresentationServiceDelegate(web_contents);

  // This object will be deleted when the RenderFrameHost is about to be
  // deleted (RenderFrameDeleted).
  PresentationServiceImpl* impl = new PresentationServiceImpl(
      render_frame_host, web_contents, controller_delegate, receiver_delegate);
  impl->Bind(std::move(request));
}

void PresentationServiceImpl::Bind(
    blink::mojom::PresentationServiceRequest request) {
  binding_.reset(new mojo::Binding<blink::mojom::PresentationService>(
      this, std::move(request)));
}

void PresentationServiceImpl::SetClient(
    blink::mojom::PresentationServiceClientPtr client) {
  DCHECK(!client_.get());
  // TODO(imcheng): Set ErrorHandler to listen for errors.
  client_ = std::move(client);

  if (receiver_delegate_) {
    receiver_delegate_->RegisterReceiverConnectionAvailableCallback(
        base::Bind(&PresentationServiceImpl::OnReceiverConnectionAvailable,
                   weak_factory_.GetWeakPtr()));
  }
}

void PresentationServiceImpl::ListenForScreenAvailability(const GURL& url) {
  DVLOG(2) << "ListenForScreenAvailability " << url.spec();
  if (!controller_delegate_) {
    client_->OnScreenAvailabilityUpdated(
        url, blink::mojom::ScreenAvailability::UNAVAILABLE);
    return;
  }

  if (screen_availability_listeners_.count(url))
    return;

  std::unique_ptr<ScreenAvailabilityListenerImpl> listener(
      new ScreenAvailabilityListenerImpl(url, this));
  if (controller_delegate_->AddScreenAvailabilityListener(
          render_process_id_, render_frame_id_, listener.get())) {
    screen_availability_listeners_[url] = std::move(listener);
  } else {
    DVLOG(1) << "AddScreenAvailabilityListener failed. Ignoring request.";
  }
}

void PresentationServiceImpl::StopListeningForScreenAvailability(
    const GURL& url) {
  DVLOG(2) << "StopListeningForScreenAvailability " << url.spec();
  if (!controller_delegate_)
    return;

  auto listener_it = screen_availability_listeners_.find(url);
  if (listener_it == screen_availability_listeners_.end())
    return;

  controller_delegate_->RemoveScreenAvailabilityListener(
      render_process_id_, render_frame_id_, listener_it->second.get());
  screen_availability_listeners_.erase(listener_it);
}

void PresentationServiceImpl::StartPresentation(
    const std::vector<GURL>& presentation_urls,
    NewPresentationCallback callback) {
  DVLOG(2) << "StartPresentation";
  if (!controller_delegate_) {
    std::move(callback).Run(
        base::nullopt,
        PresentationError(PRESENTATION_ERROR_NO_AVAILABLE_SCREENS,
                          "No screens found."));
    return;
  }

  // There is a StartPresentation request in progress. To avoid queueing up
  // requests, the incoming request is rejected.
  if (start_presentation_request_id_ != kInvalidRequestId) {
    InvokeNewPresentationCallbackWithError(std::move(callback));
    return;
  }

  start_presentation_request_id_ = GetNextRequestId();
  pending_start_presentation_cb_.reset(
      new NewPresentationCallbackWrapper(std::move(callback)));
  controller_delegate_->StartPresentation(
      render_process_id_, render_frame_id_, presentation_urls,
      base::Bind(&PresentationServiceImpl::OnStartPresentationSucceeded,
                 weak_factory_.GetWeakPtr(), start_presentation_request_id_),
      base::Bind(&PresentationServiceImpl::OnStartPresentationError,
                 weak_factory_.GetWeakPtr(), start_presentation_request_id_));
}

void PresentationServiceImpl::ReconnectPresentation(
    const std::vector<GURL>& presentation_urls,
    const base::Optional<std::string>& presentation_id,
    NewPresentationCallback callback) {
  DVLOG(2) << "ReconnectPresentation";
  if (!controller_delegate_) {
    std::move(callback).Run(
        base::nullopt,
        PresentationError(PRESENTATION_ERROR_NO_PRESENTATION_FOUND,
                          "Error joining route: No matching route"));
    return;
  }

  int request_id = RegisterReconnectPresentationCallback(&callback);
  if (request_id == kInvalidRequestId) {
    InvokeNewPresentationCallbackWithError(std::move(callback));
    return;
  }
  controller_delegate_->ReconnectPresentation(
      render_process_id_, render_frame_id_, presentation_urls,
      presentation_id.value_or(std::string()),
      base::Bind(&PresentationServiceImpl::OnReconnectPresentationSucceeded,
                 weak_factory_.GetWeakPtr(), request_id),
      base::Bind(&PresentationServiceImpl::OnReconnectPresentationError,
                 weak_factory_.GetWeakPtr(), request_id));
}

int PresentationServiceImpl::RegisterReconnectPresentationCallback(
    NewPresentationCallback* callback) {
  if (pending_reconnect_presentation_cbs_.size() >= kMaxQueuedRequests)
    return kInvalidRequestId;

  int request_id = GetNextRequestId();
  pending_reconnect_presentation_cbs_[request_id].reset(
      new NewPresentationCallbackWrapper(std::move(*callback)));
  DCHECK_NE(kInvalidRequestId, request_id);
  return request_id;
}

void PresentationServiceImpl::ListenForConnectionStateChange(
    const PresentationInfo& connection) {
  // NOTE: Blink will automatically transition the connection's state to
  // 'connected'.
  if (controller_delegate_) {
    controller_delegate_->ListenForConnectionStateChange(
        render_process_id_, render_frame_id_, connection,
        base::Bind(&PresentationServiceImpl::OnConnectionStateChanged,
                   weak_factory_.GetWeakPtr(), connection));
  }
}

void PresentationServiceImpl::OnStartPresentationSucceeded(
    int request_id,
    const PresentationInfo& presentation_info) {
  if (request_id != start_presentation_request_id_)
    return;

  CHECK(pending_start_presentation_cb_.get());
  pending_start_presentation_cb_->Run(presentation_info, base::nullopt);
  ListenForConnectionStateChange(presentation_info);
  pending_start_presentation_cb_.reset();
  start_presentation_request_id_ = kInvalidRequestId;
}

void PresentationServiceImpl::OnStartPresentationError(
    int request_id,
    const PresentationError& error) {
  if (request_id != start_presentation_request_id_)
    return;

  CHECK(pending_start_presentation_cb_.get());
  pending_start_presentation_cb_->Run(base::nullopt, error);
  pending_start_presentation_cb_.reset();
  start_presentation_request_id_ = kInvalidRequestId;
}

void PresentationServiceImpl::OnReconnectPresentationSucceeded(
    int request_id,
    const PresentationInfo& presentation_info) {
  if (RunAndEraseReconnectPresentationMojoCallback(
          request_id, presentation_info, base::nullopt)) {
    ListenForConnectionStateChange(presentation_info);
  }
}

void PresentationServiceImpl::OnReconnectPresentationError(
    int request_id,
    const PresentationError& error) {
  RunAndEraseReconnectPresentationMojoCallback(request_id, base::nullopt,
                                               error);
}

bool PresentationServiceImpl::RunAndEraseReconnectPresentationMojoCallback(
    int request_id,
    const base::Optional<PresentationInfo>& presentation_info,
    const base::Optional<PresentationError>& error) {
  auto it = pending_reconnect_presentation_cbs_.find(request_id);
  if (it == pending_reconnect_presentation_cbs_.end())
    return false;

  DCHECK(it->second.get());
  it->second->Run(presentation_info, error);
  pending_reconnect_presentation_cbs_.erase(it);
  return true;
}

void PresentationServiceImpl::SetDefaultPresentationUrls(
    const std::vector<GURL>& presentation_urls) {
  DVLOG(2) << "SetDefaultPresentationUrls";
  if (!controller_delegate_)
    return;

  if (default_presentation_urls_ == presentation_urls)
    return;

  default_presentation_urls_ = presentation_urls;
  controller_delegate_->SetDefaultPresentationUrls(
      render_process_id_, render_frame_id_, presentation_urls,
      base::Bind(&PresentationServiceImpl::OnDefaultPresentationStarted,
                 weak_factory_.GetWeakPtr()));
}

void PresentationServiceImpl::CloseConnection(
    const GURL& presentation_url,
    const std::string& presentation_id) {
  DVLOG(2) << "CloseConnection " << presentation_id;
  if (controller_delegate_)
    controller_delegate_->CloseConnection(render_process_id_, render_frame_id_,
                                          presentation_id);
}

void PresentationServiceImpl::Terminate(const GURL& presentation_url,
                                        const std::string& presentation_id) {
  DVLOG(2) << "Terminate " << presentation_id;
  if (controller_delegate_)
    controller_delegate_->Terminate(render_process_id_, render_frame_id_,
                                    presentation_id);
}

void PresentationServiceImpl::OnConnectionStateChanged(
    const PresentationInfo& connection,
    const PresentationConnectionStateChangeInfo& info) {
  DVLOG(2) << "PresentationServiceImpl::OnConnectionStateChanged "
           << "[presentation_id]: " << connection.presentation_id
           << " [state]: " << info.state;
  DCHECK(client_.get());
  if (info.state == PRESENTATION_CONNECTION_STATE_CLOSED) {
    client_->OnConnectionClosed(connection, info.close_reason, info.message);
  } else {
    client_->OnConnectionStateChanged(connection, info.state);
  }
}

bool PresentationServiceImpl::FrameMatches(
    content::RenderFrameHost* render_frame_host) const {
  if (!render_frame_host)
    return false;

  return render_frame_host->GetProcess()->GetID() == render_process_id_ &&
         render_frame_host->GetRoutingID() == render_frame_id_;
}

PresentationServiceDelegate*
PresentationServiceImpl::GetPresentationServiceDelegate() {
  return receiver_delegate_
             ? static_cast<PresentationServiceDelegate*>(receiver_delegate_)
             : static_cast<PresentationServiceDelegate*>(controller_delegate_);
}

void PresentationServiceImpl::ListenForConnectionMessages(
    const PresentationInfo& presentation_info) {
  DVLOG(2) << "ListenForConnectionMessages";
  if (!controller_delegate_)
    return;

  controller_delegate_->ListenForConnectionMessages(
      render_process_id_, render_frame_id_, presentation_info,
      base::Bind(&PresentationServiceImpl::OnConnectionMessages,
                 weak_factory_.GetWeakPtr(), presentation_info));
}

void PresentationServiceImpl::SetPresentationConnection(
    const PresentationInfo& presentation_info,
    blink::mojom::PresentationConnectionPtr controller_connection_ptr,
    blink::mojom::PresentationConnectionRequest receiver_connection_request) {
  DVLOG(2) << "SetPresentationConnection";

  if (!controller_delegate_)
    return;

  controller_delegate_->ConnectToPresentation(
      render_process_id_, render_frame_id_, presentation_info,
      std::move(controller_connection_ptr),
      std::move(receiver_connection_request));
}

void PresentationServiceImpl::OnConnectionMessages(
    const PresentationInfo& presentation_info,
    std::vector<PresentationConnectionMessage> messages) {
  DCHECK(client_);

  DVLOG(2) << "OnConnectionMessages [id]: "
           << presentation_info.presentation_id;
  client_->OnConnectionMessagesReceived(presentation_info, std::move(messages));
}

void PresentationServiceImpl::OnReceiverConnectionAvailable(
    const content::PresentationInfo& presentation_info,
    PresentationConnectionPtr controller_connection_ptr,
    PresentationConnectionRequest receiver_connection_request) {
  DVLOG(2) << "PresentationServiceImpl::OnReceiverConnectionAvailable";

  client_->OnReceiverConnectionAvailable(
      presentation_info, std::move(controller_connection_ptr),
      std::move(receiver_connection_request));
}

void PresentationServiceImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  DVLOG(2) << "PresentationServiceImpl::DidNavigateAnyFrame";
  if (!navigation_handle->HasCommitted() ||
      !FrameMatches(navigation_handle->GetRenderFrameHost())) {
    return;
  }

  std::string prev_url_host = navigation_handle->GetPreviousURL().host();
  std::string curr_url_host = navigation_handle->GetURL().host();

  // If a frame navigation is same-document (e.g. navigating to a fragment in
  // same page) then we do not unregister listeners.
  DVLOG(2) << "DidNavigateAnyFrame: "
           << "prev host: " << prev_url_host << ", curr host: " << curr_url_host
           << ", is_same_document: " << navigation_handle->IsSameDocument();
  if (navigation_handle->IsSameDocument())
    return;

  // Reset if the frame actually navigated.
  Reset();
}

void PresentationServiceImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DVLOG(2) << "PresentationServiceImpl::RenderFrameDeleted";
  if (!FrameMatches(render_frame_host))
    return;

  // RenderFrameDeleted means the associated RFH is going to be deleted soon.
  // This object should also be deleted.
  Reset();
  delete this;
}

void PresentationServiceImpl::WebContentsDestroyed() {
  LOG(ERROR) << "PresentationServiceImpl is being deleted in "
             << "WebContentsDestroyed()! This shouldn't happen since it "
             << "should've been deleted during RenderFrameDeleted().";
  Reset();
  delete this;
}

void PresentationServiceImpl::Reset() {
  DVLOG(2) << "PresentationServiceImpl::Reset";

  if (auto* delegate = GetPresentationServiceDelegate())
    delegate->Reset(render_process_id_, render_frame_id_);

  default_presentation_urls_.clear();

  screen_availability_listeners_.clear();

  start_presentation_request_id_ = kInvalidRequestId;
  pending_start_presentation_cb_.reset();

  pending_reconnect_presentation_cbs_.clear();
}

void PresentationServiceImpl::OnDelegateDestroyed() {
  DVLOG(2) << "PresentationServiceImpl::OnDelegateDestroyed";
  controller_delegate_ = nullptr;
  receiver_delegate_ = nullptr;
  Reset();
}

void PresentationServiceImpl::OnDefaultPresentationStarted(
    const PresentationInfo& connection) {
  DCHECK(client_.get());
  client_->OnDefaultPresentationStarted(connection);
  ListenForConnectionStateChange(connection);
}

PresentationServiceImpl::ScreenAvailabilityListenerImpl::
    ScreenAvailabilityListenerImpl(const GURL& availability_url,
                                   PresentationServiceImpl* service)
    : availability_url_(availability_url), service_(service) {
  DCHECK(service_);
  DCHECK(service_->client_.get());
}

PresentationServiceImpl::ScreenAvailabilityListenerImpl::
~ScreenAvailabilityListenerImpl() {
}

GURL PresentationServiceImpl::ScreenAvailabilityListenerImpl::
    GetAvailabilityUrl() const {
  return availability_url_;
}

void PresentationServiceImpl::ScreenAvailabilityListenerImpl::
    OnScreenAvailabilityChanged(blink::mojom::ScreenAvailability availability) {
  service_->client_->OnScreenAvailabilityUpdated(availability_url_,
                                                 availability);
}

void PresentationServiceImpl::ScreenAvailabilityListenerImpl
::OnScreenAvailabilityNotSupported() {
  service_->client_->OnScreenAvailabilityNotSupported(availability_url_);
}

PresentationServiceImpl::NewPresentationCallbackWrapper::
    NewPresentationCallbackWrapper(NewPresentationCallback callback)
    : callback_(std::move(callback)) {}

PresentationServiceImpl::NewPresentationCallbackWrapper::
    ~NewPresentationCallbackWrapper() {
  if (!callback_.is_null())
    InvokeNewPresentationCallbackWithError(std::move(callback_));
}

void PresentationServiceImpl::NewPresentationCallbackWrapper::Run(
    const base::Optional<PresentationInfo>& presentation_info,
    const base::Optional<PresentationError>& error) {
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(presentation_info, error);
}

}  // namespace content
