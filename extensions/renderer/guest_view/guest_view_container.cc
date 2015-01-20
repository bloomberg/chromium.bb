// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/guest_view_container.h"

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/guest_view_constants.h"

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
    : element_instance_id_(guestview::kInstanceIDNone),
      render_view_routing_id_(render_frame->GetRenderView()->GetRoutingID()),
      render_frame_(render_frame) {
  render_frame_lifetime_observer_.reset(
      new RenderFrameLifetimeObserver(this, render_frame_));
}

GuestViewContainer::~GuestViewContainer() {}

// static.
bool GuestViewContainer::HandlesMessage(const IPC::Message& msg) {
  switch (msg.type()) {
    case ExtensionMsg_CreateMimeHandlerViewGuestACK::ID:
    case ExtensionMsg_GuestAttached::ID:
    case ExtensionMsg_GuestDetached::ID:
    case ExtensionMsg_MimeHandlerViewGuestOnLoadCompleted::ID:
      return true;
    default:
      return false;
  }
}

void GuestViewContainer::RenderFrameDestroyed() {
  render_frame_ = nullptr;
}

void GuestViewContainer::SetElementInstanceID(int element_instance_id) {
  DCHECK_EQ(element_instance_id_, guestview::kInstanceIDNone);
  element_instance_id_ = element_instance_id;
}

}  // namespace extensions
