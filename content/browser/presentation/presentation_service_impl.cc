// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_service_impl.h"

#include <algorithm>

#include "base/logging.h"
#include "content/browser/presentation/presentation_type_converters.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/presentation_session_message.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/presentation_constants.h"

namespace content {

namespace {

const int kInvalidRequestSessionId = -1;

int GetNextRequestSessionId() {
  static int next_request_session_id = 0;
  return ++next_request_session_id;
}

presentation::SessionMessagePtr ToMojoSessionMessage(
    const content::PresentationSessionMessage& input) {
  presentation::SessionMessagePtr output(presentation::SessionMessage::New());
  if (input.is_binary()) {
    // binary data
    output->type = presentation::PresentationMessageType::
        PRESENTATION_MESSAGE_TYPE_ARRAY_BUFFER;
    output->data = mojo::Array<uint8_t>::From(*input.data);
  } else {
    // string message
    output->type =
        presentation::PresentationMessageType::PRESENTATION_MESSAGE_TYPE_TEXT;
    output->message = input.message;
  }
  return output.Pass();
}

scoped_ptr<PresentationSessionMessage> GetPresentationSessionMessage(
    presentation::SessionMessagePtr input) {
  DCHECK(!input.is_null());
  scoped_ptr<content::PresentationSessionMessage> output;
  switch (input->type) {
    case presentation::PRESENTATION_MESSAGE_TYPE_TEXT: {
      DCHECK(!input->message.is_null());
      DCHECK(input->data.is_null());
      // Return null PresentationSessionMessage if size exceeds.
      if (input->message.size() > content::kMaxPresentationSessionMessageSize)
        return output.Pass();

      output.reset(
          new PresentationSessionMessage(PresentationMessageType::TEXT));
      input->message.Swap(&output->message);
      return output.Pass();
    }
    case presentation::PRESENTATION_MESSAGE_TYPE_ARRAY_BUFFER: {
      DCHECK(!input->data.is_null());
      DCHECK(input->message.is_null());
      if (input->data.size() > content::kMaxPresentationSessionMessageSize)
        return output.Pass();

      output.reset(new PresentationSessionMessage(
          PresentationMessageType::ARRAY_BUFFER));
      output->data.reset(new std::vector<uint8_t>);
      input->data.Swap(output->data.get());
      return output.Pass();
    }
    case presentation::PRESENTATION_MESSAGE_TYPE_BLOB: {
      DCHECK(!input->data.is_null());
      DCHECK(input->message.is_null());
      if (input->data.size() > content::kMaxPresentationSessionMessageSize)
        return output.Pass();

      output.reset(
          new PresentationSessionMessage(PresentationMessageType::BLOB));
      output->data.reset(new std::vector<uint8_t>);
      input->data.Swap(output->data.get());
      return output.Pass();
    }
  }

  NOTREACHED() << "Invalid presentation message type " << input->type;
  return output.Pass();
}

void InvokeNewSessionMojoCallbackWithError(
    const NewSessionMojoCallback& callback) {
  callback.Run(
        presentation::PresentationSessionInfoPtr(),
        presentation::PresentationError::From(
            PresentationError(PRESENTATION_ERROR_UNKNOWN, "Internal error")));
}

} // namespace

PresentationServiceImpl::PresentationServiceImpl(
    RenderFrameHost* render_frame_host,
    WebContents* web_contents,
    PresentationServiceDelegate* delegate)
    : WebContentsObserver(web_contents),
      delegate_(delegate),
      start_session_request_id_(kInvalidRequestSessionId),
      weak_factory_(this) {
  DCHECK(render_frame_host);
  DCHECK(web_contents);

  render_process_id_ = render_frame_host->GetProcess()->GetID();
  render_frame_id_ = render_frame_host->GetRoutingID();
  DVLOG(2) << "PresentationServiceImpl: "
           << render_process_id_ << ", " << render_frame_id_;
  if (delegate_)
    delegate_->AddObserver(render_process_id_, render_frame_id_, this);
}

PresentationServiceImpl::~PresentationServiceImpl() {
  if (delegate_)
    delegate_->RemoveObserver(render_process_id_, render_frame_id_);
}

// static
void PresentationServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<presentation::PresentationService> request) {
  DVLOG(2) << "CreateMojoService";
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  // This object will be deleted when the RenderFrameHost is about to be
  // deleted (RenderFrameDeleted) or if a connection error occurred
  // (OnConnectionError).
  PresentationServiceImpl* impl = new PresentationServiceImpl(
      render_frame_host,
      web_contents,
      GetContentClient()->browser()->GetPresentationServiceDelegate(
          web_contents));
  impl->Bind(request.Pass());
}

void PresentationServiceImpl::Bind(
    mojo::InterfaceRequest<presentation::PresentationService> request) {
  binding_.reset(new mojo::Binding<presentation::PresentationService>(
      this, request.Pass()));
  binding_->set_error_handler(this);
}

void PresentationServiceImpl::OnConnectionError() {
  DVLOG(1) << "OnConnectionError";
  delete this;
}

void PresentationServiceImpl::SetClient(
    presentation::PresentationServiceClientPtr client) {
  DCHECK(!client_.get());
  // TODO(imcheng): Set ErrorHandler to listen for errors.
  client_ = client.Pass();
}

void PresentationServiceImpl::ListenForScreenAvailability() {
  DVLOG(2) << "ListenForScreenAvailability";
  if (!delegate_)
    return;

  if (screen_availability_listener_.get() &&
      screen_availability_listener_->GetPresentationUrl() ==
          default_presentation_url_) {
    return;
  }

  ResetScreenAvailabilityListener(default_presentation_url_);
}

void PresentationServiceImpl::ResetScreenAvailabilityListener(
    const std::string& presentation_url) {
  DCHECK(delegate_);
  DCHECK(!screen_availability_listener_.get() ||
         presentation_url != default_presentation_url_);

  // (1) Unregister old listener with delegate
  StopListeningForScreenAvailability();

  // (2) Replace old listener with new listener
  screen_availability_listener_.reset(new ScreenAvailabilityListenerImpl(
      presentation_url, this));

  // (3) Register new listener with delegate
  if (!delegate_->AddScreenAvailabilityListener(
      render_process_id_,
      render_frame_id_,
      screen_availability_listener_.get())) {
    DVLOG(1) << "AddScreenAvailabilityListener failed. Ignoring request.";
    screen_availability_listener_.reset();
  }
}

void PresentationServiceImpl::StopListeningForScreenAvailability() {
  DVLOG(2) << "StopListeningForScreenAvailability";
  if (!delegate_)
    return;

  if (screen_availability_listener_.get()) {
    delegate_->RemoveScreenAvailabilityListener(
        render_process_id_,
        render_frame_id_,
        screen_availability_listener_.get());
    screen_availability_listener_.reset();
  }
}

void PresentationServiceImpl::ListenForDefaultSessionStart(
    const DefaultSessionMojoCallback& callback) {
  if (!default_session_start_context_.get())
    default_session_start_context_.reset(new DefaultSessionStartContext);
  default_session_start_context_->AddCallback(callback);
}

void PresentationServiceImpl::StartSession(
    const mojo::String& presentation_url,
    const NewSessionMojoCallback& callback) {
  DVLOG(2) << "StartSession";
  if (!delegate_) {
    InvokeNewSessionMojoCallbackWithError(callback);
    return;
  }

  // There is a StartSession request in progress. To avoid queueing up
  // requests, the incoming request is rejected.
  if (start_session_request_id_ != kInvalidRequestSessionId) {
    InvokeNewSessionMojoCallbackWithError(callback);
    return;
  }

  start_session_request_id_ = GetNextRequestSessionId();
  pending_start_session_cb_.reset(new NewSessionMojoCallbackWrapper(callback));
  delegate_->StartSession(
      render_process_id_, render_frame_id_, presentation_url,
      base::Bind(&PresentationServiceImpl::OnStartSessionSucceeded,
                 weak_factory_.GetWeakPtr(), start_session_request_id_),
      base::Bind(&PresentationServiceImpl::OnStartSessionError,
                 weak_factory_.GetWeakPtr(), start_session_request_id_));
}

void PresentationServiceImpl::JoinSession(
    const mojo::String& presentation_url,
    const mojo::String& presentation_id,
    const NewSessionMojoCallback& callback) {
  DVLOG(2) << "JoinSession";
  if (!delegate_) {
    InvokeNewSessionMojoCallbackWithError(callback);
    return;
  }

  int request_session_id = RegisterJoinSessionCallback(callback);
  if (request_session_id == kInvalidRequestSessionId) {
    InvokeNewSessionMojoCallbackWithError(callback);
    return;
  }
  delegate_->JoinSession(
      render_process_id_,
      render_frame_id_,
      presentation_url,
      presentation_id,
      base::Bind(&PresentationServiceImpl::OnJoinSessionSucceeded,
                 weak_factory_.GetWeakPtr(), request_session_id),
      base::Bind(&PresentationServiceImpl::OnJoinSessionError,
                 weak_factory_.GetWeakPtr(), request_session_id));
}

int PresentationServiceImpl::RegisterJoinSessionCallback(
    const NewSessionMojoCallback& callback) {
  if (pending_join_session_cbs_.size() >= kMaxNumQueuedSessionRequests)
    return kInvalidRequestSessionId;

  int request_id = GetNextRequestSessionId();
  pending_join_session_cbs_[request_id].reset(
      new NewSessionMojoCallbackWrapper(callback));
  return request_id;
}

void PresentationServiceImpl::OnStartSessionSucceeded(
    int request_session_id,
    const PresentationSessionInfo& session_info) {
  if (request_session_id == start_session_request_id_) {
    CHECK(pending_start_session_cb_.get());
    pending_start_session_cb_->Run(
        presentation::PresentationSessionInfo::From(session_info),
        presentation::PresentationErrorPtr());
    pending_start_session_cb_.reset();
    start_session_request_id_ = kInvalidRequestSessionId;
  }
}

void PresentationServiceImpl::OnStartSessionError(
    int request_session_id,
    const PresentationError& error) {
  if (request_session_id == start_session_request_id_) {
    CHECK(pending_start_session_cb_.get());
    pending_start_session_cb_->Run(
        presentation::PresentationSessionInfoPtr(),
        presentation::PresentationError::From(error));
    pending_start_session_cb_.reset();
    start_session_request_id_ = kInvalidRequestSessionId;
  }
}

void PresentationServiceImpl::OnJoinSessionSucceeded(
    int request_session_id,
    const PresentationSessionInfo& session_info) {
  RunAndEraseJoinSessionMojoCallback(
      request_session_id,
      presentation::PresentationSessionInfo::From(session_info),
      presentation::PresentationErrorPtr());
}

void PresentationServiceImpl::OnJoinSessionError(
    int request_session_id,
    const PresentationError& error) {
  RunAndEraseJoinSessionMojoCallback(
      request_session_id,
      presentation::PresentationSessionInfoPtr(),
      presentation::PresentationError::From(error));
}

void PresentationServiceImpl::RunAndEraseJoinSessionMojoCallback(
    int request_session_id,
    presentation::PresentationSessionInfoPtr session,
    presentation::PresentationErrorPtr error) {
  auto it = pending_join_session_cbs_.find(request_session_id);
  if (it == pending_join_session_cbs_.end())
    return;

  DCHECK(it->second.get());
  it->second->Run(session.Pass(), error.Pass());
  pending_join_session_cbs_.erase(it);
}

void PresentationServiceImpl::SetDefaultPresentationURL(
    const mojo::String& default_presentation_url) {
  DVLOG(2) << "SetDefaultPresentationURL";
  if (!delegate_)
    return;

  const std::string& old_default_url = default_presentation_url_;
  const std::string& new_default_url = default_presentation_url.get();

  if (old_default_url == new_default_url)
    return;

  // Replace screen availability listeners if any.
  if (screen_availability_listener_.get())
    ResetScreenAvailabilityListener(new_default_url);

  delegate_->SetDefaultPresentationUrl(
      render_process_id_,
      render_frame_id_,
      default_presentation_url);
  default_presentation_url_ = default_presentation_url;
}

void PresentationServiceImpl::SendSessionMessage(
    presentation::PresentationSessionInfoPtr session,
    presentation::SessionMessagePtr session_message,
    const SendMessageMojoCallback& callback) {
  DVLOG(2) << "SendSessionMessage";
  DCHECK(!session_message.is_null());
  // send_message_callback_ should be null by now, otherwise resetting of
  // send_message_callback_ with new callback will drop the old callback.
  if (!delegate_ || send_message_callback_) {
    callback.Run(false);
    return;
  }

  send_message_callback_.reset(new SendMessageMojoCallback(callback));
  delegate_->SendMessage(
      render_process_id_, render_frame_id_,
      session.To<PresentationSessionInfo>(),
      GetPresentationSessionMessage(session_message.Pass()),
      base::Bind(&PresentationServiceImpl::OnSendMessageCallback,
                 weak_factory_.GetWeakPtr()));
}

void PresentationServiceImpl::OnSendMessageCallback(bool sent) {
  // It is possible that Reset() is invoked before receiving this callback.
  // So, always check send_message_callback_ for non-null.
  if (send_message_callback_) {
    send_message_callback_->Run(sent);
    send_message_callback_.reset();
  }
}

void PresentationServiceImpl::CloseSession(
    const mojo::String& presentation_url,
    const mojo::String& presentation_id) {
  DVLOG(2) << "CloseSession " << presentation_id;
  if (delegate_)
    delegate_->CloseSession(render_process_id_, render_frame_id_,
                            presentation_id);
}

void PresentationServiceImpl::ListenForSessionStateChange() {
  if (!delegate_)
    return;

  delegate_->ListenForSessionStateChange(
      render_process_id_, render_frame_id_,
      base::Bind(&PresentationServiceImpl::OnSessionStateChanged,
                 weak_factory_.GetWeakPtr()));
}

void PresentationServiceImpl::OnSessionStateChanged(
    const PresentationSessionInfo& session_info,
    PresentationSessionState session_state) {
  DCHECK(client_.get());
  client_->OnSessionStateChanged(
      presentation::PresentationSessionInfo::From(session_info),
      PresentationSessionStateToMojo(session_state));
}

bool PresentationServiceImpl::FrameMatches(
    content::RenderFrameHost* render_frame_host) const {
  if (!render_frame_host)
    return false;

  return render_frame_host->GetProcess()->GetID() == render_process_id_ &&
         render_frame_host->GetRoutingID() == render_frame_id_;
}

void PresentationServiceImpl::ListenForSessionMessages(
    presentation::PresentationSessionInfoPtr session) {
  DVLOG(2) << "ListenForSessionMessages";
  if (!delegate_)
    return;

  PresentationSessionInfo session_info(session.To<PresentationSessionInfo>());
  delegate_->ListenForSessionMessages(
      render_process_id_, render_frame_id_, session_info,
      base::Bind(&PresentationServiceImpl::OnSessionMessages,
                 weak_factory_.GetWeakPtr(), session_info));
}

void PresentationServiceImpl::OnSessionMessages(
    const PresentationSessionInfo& session,
    const ScopedVector<PresentationSessionMessage>& messages) {
  DCHECK(client_);

  DVLOG(2) << "OnSessionMessages";
  mojo::Array<presentation::SessionMessagePtr> mojoMessages(messages.size());
  for (size_t i = 0; i < messages.size(); ++i)
    mojoMessages[i] = ToMojoSessionMessage(*messages[i]);

  client_->OnSessionMessagesReceived(
      presentation::PresentationSessionInfo::From(session),
      mojoMessages.Pass());
}

void PresentationServiceImpl::DidNavigateAnyFrame(
    content::RenderFrameHost* render_frame_host,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  DVLOG(2) << "PresentationServiceImpl::DidNavigateAnyFrame";
  if (!FrameMatches(render_frame_host))
    return;

  std::string prev_url_host = details.previous_url.host();
  std::string curr_url_host = params.url.host();

  // If a frame navigation is in-page (e.g. navigating to a fragment in
  // same page) then we do not unregister listeners.
  DVLOG(2) << "DidNavigateAnyFrame: "
           << "prev host: " << prev_url_host << ", curr host: " << curr_url_host
           << ", details.is_in_page: " << details.is_in_page;
  if (details.is_in_page)
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

void PresentationServiceImpl::Reset() {
  DVLOG(2) << "PresentationServiceImpl::Reset";
  if (delegate_)
    delegate_->Reset(render_process_id_, render_frame_id_);

  default_presentation_url_.clear();

  screen_availability_listener_.reset();

  start_session_request_id_ = kInvalidRequestSessionId;
  pending_start_session_cb_.reset();

  pending_join_session_cbs_.clear();

  default_session_start_context_.reset();

  if (on_session_messages_callback_.get()) {
    on_session_messages_callback_->Run(
        mojo::Array<presentation::SessionMessagePtr>());
    on_session_messages_callback_.reset();
  }

  if (send_message_callback_) {
    // Run the callback with false, indicating the renderer to stop sending
    // the requests and invalidate all pending requests.
    send_message_callback_->Run(false);
    send_message_callback_.reset();
  }
}

void PresentationServiceImpl::OnDelegateDestroyed() {
  DVLOG(2) << "PresentationServiceImpl::OnDelegateDestroyed";
  delegate_ = nullptr;
  Reset();
}

void PresentationServiceImpl::OnDefaultPresentationStarted(
    const PresentationSessionInfo& session) {
  if (default_session_start_context_.get())
    default_session_start_context_->set_session(session);
}

PresentationServiceImpl::ScreenAvailabilityListenerImpl
::ScreenAvailabilityListenerImpl(
    const std::string& presentation_url,
    PresentationServiceImpl* service)
    : presentation_url_(presentation_url),
      service_(service) {
  DCHECK(service_);
  DCHECK(service_->client_.get());
}

PresentationServiceImpl::ScreenAvailabilityListenerImpl::
~ScreenAvailabilityListenerImpl() {
}

std::string PresentationServiceImpl::ScreenAvailabilityListenerImpl
    ::GetPresentationUrl() const {
  return presentation_url_;
}

void PresentationServiceImpl::ScreenAvailabilityListenerImpl
    ::OnScreenAvailabilityChanged(bool available) {
  service_->client_->OnScreenAvailabilityUpdated(available);
}

PresentationServiceImpl::NewSessionMojoCallbackWrapper
::NewSessionMojoCallbackWrapper(const NewSessionMojoCallback& callback)
    : callback_(callback) {
}

PresentationServiceImpl::NewSessionMojoCallbackWrapper
::~NewSessionMojoCallbackWrapper() {
  if (!callback_.is_null())
    InvokeNewSessionMojoCallbackWithError(callback_);
}

void PresentationServiceImpl::NewSessionMojoCallbackWrapper::Run(
    presentation::PresentationSessionInfoPtr session,
    presentation::PresentationErrorPtr error) {
  DCHECK(!callback_.is_null());
  callback_.Run(session.Pass(), error.Pass());
  callback_.reset();
}

PresentationServiceImpl::DefaultSessionStartContext
::DefaultSessionStartContext() {
}

PresentationServiceImpl::DefaultSessionStartContext
::~DefaultSessionStartContext() {
  Reset();
}

void PresentationServiceImpl::DefaultSessionStartContext::AddCallback(
    const DefaultSessionMojoCallback& callback) {
  if (session_.get()) {
    DCHECK(callbacks_.empty());
    callback.Run(presentation::PresentationSessionInfo::From(*session_));
    session_.reset();
  } else {
    callbacks_.push_back(new DefaultSessionMojoCallback(callback));
  }
}

void PresentationServiceImpl::DefaultSessionStartContext::set_session(
    const PresentationSessionInfo& session) {
  if (callbacks_.empty()) {
    session_.reset(new PresentationSessionInfo(session));
  } else {
    DCHECK(!session_.get());
    ScopedVector<DefaultSessionMojoCallback> callbacks;
    callbacks.swap(callbacks_);
    for (const auto& callback : callbacks)
      callback->Run(presentation::PresentationSessionInfo::From(session));
  }
}

void PresentationServiceImpl::DefaultSessionStartContext::Reset() {
  ScopedVector<DefaultSessionMojoCallback> callbacks;
  callbacks.swap(callbacks_);
  for (const auto& callback : callbacks)
    callback->Run(presentation::PresentationSessionInfoPtr());
  session_.reset();
}

}  // namespace content
