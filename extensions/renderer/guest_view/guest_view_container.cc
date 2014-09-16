// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/guest_view_container.h"

#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/guest_view_constants.h"

namespace extensions {

GuestViewContainer::GuestViewContainer(
    content::RenderFrame* render_frame,
    const std::string& mime_type)
    : content::BrowserPluginDelegate(render_frame, mime_type),
      content::RenderFrameObserver(render_frame),
      mime_type_(mime_type),
      element_instance_id_(guestview::kInstanceIDNone) {
}

GuestViewContainer::~GuestViewContainer() {
}

void GuestViewContainer::SetElementInstanceID(int element_instance_id) {
  element_instance_id_ = element_instance_id;
}

void GuestViewContainer::DidFinishLoading() {
  if (mime_type_.empty())
    return;

  DCHECK_NE(element_instance_id_, guestview::kInstanceIDNone);
  render_frame()->Send(new ExtensionHostMsg_CreateMimeHandlerViewGuest(
      routing_id(), html_string_, mime_type_, element_instance_id_));
}

void GuestViewContainer::DidReceiveData(const char* data, int data_length) {
  std::string value(data, data_length);
  html_string_ += value;
}

void GuestViewContainer::OnDestruct() {
  // GuestViewContainer's lifetime is managed by BrowserPlugin so don't let
  // RenderFrameObserver self-destruct here.
}

bool GuestViewContainer::OnMessageReceived(const IPC::Message& message) {
  if (message.type() != ExtensionMsg_CreateMimeHandlerViewGuestACK::ID)
    return false;

  DCHECK_NE(element_instance_id_, guestview::kInstanceIDNone);
  int element_instance_id = guestview::kInstanceIDNone;
  PickleIterator iter(message);
  bool success = iter.ReadInt(&element_instance_id);
  DCHECK(success);
  if (element_instance_id != element_instance_id_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GuestViewContainer, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_CreateMimeHandlerViewGuestACK,
                        OnCreateMimeHandlerViewGuestACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GuestViewContainer::OnCreateMimeHandlerViewGuestACK(
    int element_instance_id) {
  DCHECK_NE(element_instance_id_, guestview::kInstanceIDNone);
  DCHECK_EQ(element_instance_id_, element_instance_id);
  DCHECK(!mime_type_.empty());
  render_frame()->AttachGuest(element_instance_id);
}

}  // namespace extensions
