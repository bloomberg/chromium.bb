// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_service_impl.h"

#include "base/logging.h"
#include "content/browser/presentation/presentation_type_converters.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/frame_navigate_params.h"

namespace content {

PresentationServiceImpl::PresentationServiceImpl(
    RenderFrameHost* render_frame_host,
    WebContents* web_contents,
    PresentationServiceDelegate* delegate)
    : WebContentsObserver(web_contents),
      render_frame_host_(render_frame_host),
      delegate_(delegate),
      weak_factory_(this) {
  DCHECK(render_frame_host_);
  DCHECK(web_contents);
  DVLOG(2) << "PresentationServiceImpl: "
           << render_frame_host_->GetProcess()->GetID() << ", "
           << render_frame_host_->GetRoutingID();
  if (delegate_)
    delegate_->AddObserver(this);
}

PresentationServiceImpl::~PresentationServiceImpl() {
  if (delegate_)
    delegate_->RemoveObserver(this);
}

// static
void PresentationServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<presentation::PresentationService> request) {
  DVLOG(2) << "CreateMojoService";
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  mojo::BindToRequest(
      new PresentationServiceImpl(
          render_frame_host,
          web_contents,
          GetContentClient()->browser()->GetPresentationServiceDelegate(
              web_contents)),
          &request);
}

void PresentationServiceImpl::OnConnectionError() {
  DVLOG(1) << "OnConnectionError: "
           << render_frame_host_->GetProcess()->GetID() << ", "
           << render_frame_host_->GetRoutingID();
}

PresentationServiceImpl::ScreenAvailabilityContext*
PresentationServiceImpl::GetOrCreateAvailabilityContext(
    const std::string& presentation_url) {
  auto it = availability_contexts_.find(presentation_url);
  if (it == availability_contexts_.end()) {
    linked_ptr<ScreenAvailabilityContext> context(
        new ScreenAvailabilityContext(presentation_url));
    if (!delegate_->AddScreenAvailabilityListener(
        render_frame_host_->GetProcess()->GetID(),
        render_frame_host_->GetRoutingID(),
        context.get())) {
      DVLOG(1) << "AddScreenAvailabilityListener failed. Ignoring request.";
      return nullptr;
    }
    it = availability_contexts_.insert(
        std::make_pair(context->GetPresentationUrl(), context)).first;
  }
  return it->second.get();
}

void PresentationServiceImpl::GetScreenAvailability(
    const mojo::String& presentation_url,
    const ScreenAvailabilityMojoCallback& callback) {
  DVLOG(2) << "GetScreenAvailability";
  if (!delegate_)
    return;

  ScreenAvailabilityContext* context =
      GetOrCreateAvailabilityContext(presentation_url.get());
  if (!context)
    return;
  context->CallbackReceived(callback);
}

void PresentationServiceImpl::OnScreenAvailabilityListenerRemoved(
    const mojo::String& presentation_url) {
  DVLOG(2) << "OnScreenAvailabilityListenerRemoved";
  if (!delegate_)
    return;

  const std::string& presentation_url_str = presentation_url.get();
  auto it = availability_contexts_.find(presentation_url_str);
  if (it == availability_contexts_.end())
    return;

  delegate_->RemoveScreenAvailabilityListener(
      render_frame_host_->GetProcess()->GetID(),
      render_frame_host_->GetRoutingID(),
      it->second.get());
  availability_contexts_.erase(it);
}

void PresentationServiceImpl::ListenForDefaultSessionStart(
    const DefaultSessionMojoCallback& callback) {
  NOTIMPLEMENTED();
}

void PresentationServiceImpl::StartSession(
    const mojo::String& presentation_url,
    const mojo::String& presentation_id,
    const NewSessionMojoCallback& callback) {
  DVLOG(2) << "StartSession";
  if (!delegate_)
    return;

  queued_start_session_requests_.push_back(make_linked_ptr(
      new StartSessionRequest(presentation_url, presentation_id, callback)));
  if (queued_start_session_requests_.size() == 1)
    DoStartSession(presentation_url, presentation_id, callback);
}

void PresentationServiceImpl::JoinSession(
    const mojo::String& presentation_url,
    const mojo::String& presentation_id,
    const NewSessionMojoCallback& callback) {
  DVLOG(2) << "JoinSession";
  if (!delegate_)
    return;

  delegate_->JoinSession(
      render_frame_host_->GetProcess()->GetID(),
      render_frame_host_->GetRoutingID(),
      presentation_url,
      presentation_id,
      base::Bind(&PresentationServiceImpl::OnStartOrJoinSessionSucceeded,
                 weak_factory_.GetWeakPtr(), false, callback),
      base::Bind(&PresentationServiceImpl::OnStartOrJoinSessionError,
                 weak_factory_.GetWeakPtr(), false, callback));
}

void PresentationServiceImpl::HandleQueuedStartSessionRequests() {
  DCHECK(!queued_start_session_requests_.empty());
  queued_start_session_requests_.pop_front();
  if (!queued_start_session_requests_.empty()) {
    const linked_ptr<StartSessionRequest>& request =
        queued_start_session_requests_.front();
    DoStartSession(request->presentation_url,
                   request->presentation_id,
                   request->callback);
  }
}

void PresentationServiceImpl::DoStartSession(
    const std::string& presentation_url,
    const std::string& presentation_id,
    const NewSessionMojoCallback& callback) {
  delegate_->StartSession(
      render_frame_host_->GetProcess()->GetID(),
      render_frame_host_->GetRoutingID(),
      presentation_url,
      presentation_id,
      base::Bind(&PresentationServiceImpl::OnStartOrJoinSessionSucceeded,
                 weak_factory_.GetWeakPtr(), true, callback),
      base::Bind(&PresentationServiceImpl::OnStartOrJoinSessionError,
                 weak_factory_.GetWeakPtr(), true, callback));
}

void PresentationServiceImpl::OnStartOrJoinSessionSucceeded(
    bool is_start_session,
    const NewSessionMojoCallback& callback,
    const PresentationSessionInfo& session_info) {
  callback.Run(
      presentation::PresentationSessionInfo::From(session_info),
      presentation::PresentationErrorPtr());
  if (is_start_session)
    HandleQueuedStartSessionRequests();
}

void PresentationServiceImpl::OnStartOrJoinSessionError(
    bool is_start_session,
    const NewSessionMojoCallback& callback,
    const PresentationError& error) {
  callback.Run(
      presentation::PresentationSessionInfoPtr(),
      presentation::PresentationError::From(error));
  if (is_start_session)
    HandleQueuedStartSessionRequests();
}

void PresentationServiceImpl::DoSetDefaultPresentationUrl(
    const std::string& default_presentation_url,
    const std::string& default_presentation_id) {
  DCHECK(delegate_);
  delegate_->SetDefaultPresentationUrl(
      render_frame_host_->GetProcess()->GetID(),
      render_frame_host_->GetRoutingID(),
      default_presentation_url,
      default_presentation_id);
  default_presentation_url_ = default_presentation_url;
  default_presentation_id_ = default_presentation_id;
}

void PresentationServiceImpl::SetDefaultPresentationURL(
    const mojo::String& default_presentation_url,
    const mojo::String& default_presentation_id) {
  DVLOG(2) << "SetDefaultPresentationURL";
  if (!delegate_)
    return;

  const std::string& old_default_url = default_presentation_url_;
  const std::string& new_default_url = default_presentation_url.get();

  // Don't call delegate if nothing changed.
  if (old_default_url == new_default_url &&
      default_presentation_id_ == default_presentation_id) {
    return;
  }

  auto old_it = availability_contexts_.find(old_default_url);
  // Haven't started listening yet.
  if (old_it == availability_contexts_.end()) {
    DoSetDefaultPresentationUrl(new_default_url, default_presentation_id);
    return;
  }

  // Have already started listening. Create a listener for the new URL and
  // transfer the callback from the old listener, if any.
  // This is done so that a listener added before default URL is changed
  // will continue to work.
  ScreenAvailabilityMojoCallback* old_callback = old_it->second->GetCallback();
  ScreenAvailabilityContext* context =
      GetOrCreateAvailabilityContext(new_default_url);
  if (old_callback)
    context->CallbackReceived(*old_callback);

  // Remove listener for old default presentation URL.
  delegate_->RemoveScreenAvailabilityListener(
      render_frame_host_->GetProcess()->GetID(),
      render_frame_host_->GetRoutingID(),
      old_it->second.get());
  availability_contexts_.erase(old_it);
  DoSetDefaultPresentationUrl(new_default_url, default_presentation_id);
}

void PresentationServiceImpl::CloseSession(
    const mojo::String& presentation_url,
    const mojo::String& presentation_id) {
  NOTIMPLEMENTED();
}

void PresentationServiceImpl::DidNavigateAnyFrame(
    content::RenderFrameHost* render_frame_host,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  DVLOG(2) << "PresentationServiceImpl::DidNavigateAnyFrame";
  if (render_frame_host_ != render_frame_host)
    return;

  std::string prev_url_host = details.previous_url.host();
  std::string curr_url_host = params.url.host();

  // If a frame navigation is in-page (e.g. navigating to a fragment in
  // same page) then we do not unregister listeners.
  bool in_page_navigation = details.is_in_page ||
      details.type == content::NAVIGATION_TYPE_IN_PAGE;

  DVLOG(2) << "DidNavigateAnyFrame: "
          << "prev host: " << prev_url_host << ", curr host: " << curr_url_host
          << ", in_page_navigation: " << in_page_navigation;

  if (in_page_navigation)
    return;

  // Reset if the frame actually navigated.
  Reset();
}

void PresentationServiceImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DVLOG(2) << "PresentationServiceImpl::RenderFrameDeleted";
  if (render_frame_host_ != render_frame_host)
    return;

  // RenderFrameDeleted means this object is getting deleted soon.
  Reset();
}

void PresentationServiceImpl::Reset() {
  DVLOG(2) << "PresentationServiceImpl::Reset";
  if (delegate_) {
    delegate_->Reset(
        render_frame_host_->GetProcess()->GetID(),
        render_frame_host_->GetRoutingID());
  }

  default_presentation_url_.clear();
  default_presentation_id_.clear();
  availability_contexts_.clear();
  queued_start_session_requests_.clear();
}

void PresentationServiceImpl::OnDelegateDestroyed() {
  DVLOG(2) << "PresentationServiceImpl::OnDelegateDestroyed";
  delegate_ = nullptr;
  Reset();
}

PresentationServiceImpl::ScreenAvailabilityContext::ScreenAvailabilityContext(
    const std::string& presentation_url)
    : presentation_url_(presentation_url) {
}

PresentationServiceImpl::ScreenAvailabilityContext::
~ScreenAvailabilityContext() {
}

void PresentationServiceImpl::ScreenAvailabilityContext::CallbackReceived(
    const ScreenAvailabilityMojoCallback& callback) {
  // NOTE: This will overwrite previously registered callback if any.
  if (!available_ptr_) {
    // No results yet, store callback for later invocation.
    callback_ptr_.reset(new ScreenAvailabilityMojoCallback(callback));
  } else {
    // Run callback now, reset result.
    // There shouldn't be any callbacks stored in this scenario.
    DCHECK(!callback_ptr_);
    callback.Run(presentation_url_, *available_ptr_);
    Reset();
  }
}

void PresentationServiceImpl::ScreenAvailabilityContext::Reset() {
  callback_ptr_.reset();
  available_ptr_.reset();
}

std::string PresentationServiceImpl::ScreenAvailabilityContext
    ::GetPresentationUrl() const {
  return presentation_url_;
}

void PresentationServiceImpl::ScreenAvailabilityContext
    ::OnScreenAvailabilityChanged(bool available) {
  if (!callback_ptr_) {
    // No callback, stash the result for now.
    available_ptr_.reset(new bool(available));
  } else {
    // Invoke callback and erase it.
    // There shouldn't be any result stored in this scenario.
    DCHECK(!available_ptr_);
    callback_ptr_->Run(presentation_url_, available);
    Reset();
  }
}

PresentationServiceImpl::ScreenAvailabilityMojoCallback*
PresentationServiceImpl::ScreenAvailabilityContext::GetCallback() const {
  return callback_ptr_.get();
}

PresentationServiceImpl::StartSessionRequest::StartSessionRequest(
    const std::string& presentation_url,
    const std::string& presentation_id,
    const NewSessionMojoCallback& callback)
    : presentation_url(presentation_url),
      presentation_id(presentation_id),
      callback(callback) {
}

PresentationServiceImpl::StartSessionRequest::~StartSessionRequest() {
}

}  // namespace content
