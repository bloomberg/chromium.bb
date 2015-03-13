// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_service_impl.h"

#include "base/logging.h"
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
      delegate_(delegate) {
  DCHECK(render_frame_host_);
  DCHECK(web_contents);
  VLOG(2) << "PresentationServiceImpl: "
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
  VLOG(2) << "PresentationServiceImpl::CreateService";
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
  VLOG(1) << "PresentationServiceImpl::OnConnectionError: "
          << render_frame_host_->GetProcess()->GetID() << ", "
          << render_frame_host_->GetRoutingID();
}

void PresentationServiceImpl::SetDefaultPresentationURL(
    const mojo::String& presentation_url,
    const mojo::String& presentation_id) {
  NOTIMPLEMENTED();
}


void PresentationServiceImpl::GetScreenAvailability(
    const mojo::String& presentation_url,
    const ScreenAvailabilityMojoCallback& callback) {
  VLOG(2) << "PresentationServiceImpl::GetScreenAvailability";
  if (!delegate_)
    return;

  const std::string& presentation_url_str = !presentation_url.is_null() ?
      presentation_url.get() : default_presentation_url_;

  // GetScreenAvailability() is called with no URL and there is no default
  // Presentation URL.
  if (presentation_url_str.empty())
    return;

  auto it = availability_contexts_.find(presentation_url_str);
  if (it == availability_contexts_.end()) {
    linked_ptr<ScreenAvailabilityContext> context(
        new ScreenAvailabilityContext(presentation_url_str));

    if (!delegate_->AddScreenAvailabilityListener(
        render_frame_host_->GetProcess()->GetID(),
        render_frame_host_->GetRoutingID(),
        context.get())) {
      VLOG(1) << "AddScreenAvailabilityListener failed. Ignoring request.";
      return;
    }

    it = availability_contexts_.insert(
        std::make_pair(presentation_url_str, context)).first;
  }

  it->second->CallbackReceived(callback);
}

void PresentationServiceImpl::OnScreenAvailabilityListenerRemoved(
    const mojo::String& presentation_url) {
  NOTIMPLEMENTED();
}

void PresentationServiceImpl::ListenForDefaultSessionStart(
    const DefaultSessionMojoCallback& callback) {
  NOTIMPLEMENTED();
}

void PresentationServiceImpl::StartSession(
    const mojo::String& presentation_url,
    const mojo::String& presentation_id,
    const NewSessionMojoCallback& callback) {
  NOTIMPLEMENTED();
}

void PresentationServiceImpl::JoinSession(
    const mojo::String& presentation_url,
    const mojo::String& presentation_id,
    const NewSessionMojoCallback& callback) {
  NOTIMPLEMENTED();
}

void PresentationServiceImpl::DidNavigateAnyFrame(
    content::RenderFrameHost* render_frame_host,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  VLOG(2) << "PresentationServiceImpl::DidNavigateAnyFrame";
  if (render_frame_host_ != render_frame_host)
    return;

  std::string prev_url_host = details.previous_url.host();
  std::string curr_url_host = params.url.host();

  // If a frame navigation is in-page (e.g. navigating to a fragment in
  // same page) then we do not unregister listeners.
  bool in_page_navigation = details.is_in_page ||
      details.type == content::NAVIGATION_TYPE_IN_PAGE;

  VLOG(2) << "DidNavigateAnyFrame: "
          << "prev host: " << prev_url_host << ", curr host: " << curr_url_host
          << ", in_page_navigation: " << in_page_navigation;

  if (in_page_navigation)
    return;

  // Unregister all sources if the frame actually navigated.
  RemoveAllListeners();
}

void PresentationServiceImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  VLOG(2) << "PresentationServiceImpl::RenderFrameDeleted";
  if (render_frame_host_ != render_frame_host)
    return;

  // RenderFrameDeleted means this object is getting deleted soon.
  RemoveAllListeners();
}

void PresentationServiceImpl::RemoveAllListeners() {
  VLOG(2) << "PresentationServiceImpl::RemoveAllListeners";
  if (!delegate_)
    return;

  delegate_->RemoveAllScreenAvailabilityListeners(
        render_frame_host_->GetProcess()->GetID(),
        render_frame_host_->GetRoutingID());

  availability_contexts_.clear();
}

void PresentationServiceImpl::OnDelegateDestroyed() {
  VLOG(2) << "PresentationServiceImpl::OnDelegateDestroyed";
  delegate_ = nullptr;
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
    callback.Run(*available_ptr_);
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
    callback_ptr_->Run(available);
    Reset();
  }
}

}  // namespace content
