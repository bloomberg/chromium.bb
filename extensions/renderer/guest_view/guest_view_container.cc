// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/guest_view_container.h"

#include "components/guest_view/common/guest_view_constants.h"
#include "components/guest_view/common/guest_view_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "extensions/renderer/guest_view/guest_view_request.h"

namespace {

using GuestViewContainerMap = std::map<int, extensions::GuestViewContainer*>;
static base::LazyInstance<GuestViewContainerMap> g_guest_view_container_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

class GuestViewContainer::RenderFrameLifetimeObserver
    : public content::RenderFrameObserver {
 public:
  RenderFrameLifetimeObserver(GuestViewContainer* container,
                              content::RenderFrame* render_frame);

  // content::RenderFrameObserver overrides.
  void OnDestruct() override;

 private:
  GuestViewContainer* container_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameLifetimeObserver);
};

GuestViewContainer::RenderFrameLifetimeObserver::RenderFrameLifetimeObserver(
    GuestViewContainer* container,
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      container_(container) {}

void GuestViewContainer::RenderFrameLifetimeObserver::OnDestruct() {
  container_->RenderFrameDestroyed();
}

GuestViewContainer::GuestViewContainer(content::RenderFrame* render_frame)
    : element_instance_id_(guest_view::kInstanceIDNone),
      render_frame_(render_frame),
      ready_(false) {
  render_frame_lifetime_observer_.reset(
      new RenderFrameLifetimeObserver(this, render_frame_));
}

GuestViewContainer::~GuestViewContainer() {
  if (element_instance_id() != guest_view::kInstanceIDNone)
    g_guest_view_container_map.Get().erase(element_instance_id());

  if (pending_response_.get())
    pending_response_->ExecuteCallbackIfAvailable(0 /* argc */, nullptr);

  while (pending_requests_.size() > 0) {
    linked_ptr<GuestViewRequest> pending_request = pending_requests_.front();
    pending_requests_.pop_front();
    // Call the JavaScript callbacks with no arguments which implies an error.
    pending_request->ExecuteCallbackIfAvailable(0 /* argc */, nullptr);
  }
}

// static.
GuestViewContainer* GuestViewContainer::FromID(int element_instance_id) {
  GuestViewContainerMap* guest_view_containers =
      g_guest_view_container_map.Pointer();
  auto it = guest_view_containers->find(element_instance_id);
  return it == guest_view_containers->end() ? nullptr : it->second;
}

void GuestViewContainer::RenderFrameDestroyed() {
  OnRenderFrameDestroyed();
  render_frame_ = nullptr;
}

void GuestViewContainer::IssueRequest(linked_ptr<GuestViewRequest> request) {
  EnqueueRequest(request);
  PerformPendingRequest();
}

void GuestViewContainer::EnqueueRequest(linked_ptr<GuestViewRequest> request) {
  pending_requests_.push_back(request);
}

void GuestViewContainer::PerformPendingRequest() {
  if (!ready_ || pending_requests_.empty() || pending_response_.get())
    return;

  linked_ptr<GuestViewRequest> pending_request = pending_requests_.front();
  pending_requests_.pop_front();
  pending_request->PerformRequest();
  pending_response_ = pending_request;
}

void GuestViewContainer::HandlePendingResponseCallback(
    const IPC::Message& message) {
  CHECK(pending_response_.get());
  linked_ptr<GuestViewRequest> pending_response(pending_response_.release());
  pending_response->HandleResponse(message);
}

void GuestViewContainer::OnHandleCallback(const IPC::Message& message) {
  // Handle the callback for the current request with a pending response.
  HandlePendingResponseCallback(message);
  // Perform the subsequent request if one exists.
  PerformPendingRequest();
}

bool GuestViewContainer::OnMessage(const IPC::Message& message) {
  return false;
}

bool GuestViewContainer::OnMessageReceived(const IPC::Message& message) {
  if (OnMessage(message))
    return true;

  OnHandleCallback(message);
  return true;
}

void GuestViewContainer::Ready() {
  ready_ = true;
  CHECK(!pending_response_.get());
  PerformPendingRequest();

  // Give the derived type an opportunity to perform some actions when the
  // container acquires a geometry.
  OnReady();
}

void GuestViewContainer::SetElementInstanceID(int element_instance_id) {
  DCHECK_EQ(element_instance_id_, guest_view::kInstanceIDNone);
  element_instance_id_ = element_instance_id;

  DCHECK(!g_guest_view_container_map.Get().count(element_instance_id));
  g_guest_view_container_map.Get().insert(
      std::make_pair(element_instance_id, this));
}

}  // namespace extensions
