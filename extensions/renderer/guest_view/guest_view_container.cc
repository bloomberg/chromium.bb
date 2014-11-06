// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/guest_view_container.h"

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/guest_view/guest_view_constants.h"

namespace extensions {

GuestViewContainer::GuestViewContainer(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      element_instance_id_(guestview::kInstanceIDNone),
      render_view_routing_id_(render_frame->GetRenderView()->GetRoutingID()) {
}

GuestViewContainer::~GuestViewContainer() {}

void GuestViewContainer::SetElementInstanceID(int element_instance_id) {
  DCHECK_EQ(element_instance_id_, guestview::kInstanceIDNone);
  element_instance_id_ = element_instance_id;
}

void GuestViewContainer::OnDestruct() {
  // GuestViewContainer's lifetime is managed by BrowserPlugin so don't let
  // RenderFrameObserver self-destruct here.
}

bool GuestViewContainer::OnMessageReceived(
    const IPC::Message& message) {
  if (!HandlesMessage(message))
    return false;

  DCHECK_NE(element_instance_id_, guestview::kInstanceIDNone);
  int element_instance_id = guestview::kInstanceIDNone;
  PickleIterator iter(message);
  bool success = iter.ReadInt(&element_instance_id);
  DCHECK(success);
  if (element_instance_id != element_instance_id_)
    return false;

  return OnMessage(message);
}

}  // namespace extensions
