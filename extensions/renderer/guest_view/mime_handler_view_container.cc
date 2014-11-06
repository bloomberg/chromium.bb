// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view_container.h"

#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/guest_view_constants.h"

namespace extensions {

MimeHandlerViewContainer::MimeHandlerViewContainer(
    content::RenderFrame* render_frame,
    const std::string& mime_type)
    : GuestViewContainer(render_frame),
      mime_type_(mime_type) {
  DCHECK(!mime_type_.empty());
}

MimeHandlerViewContainer::~MimeHandlerViewContainer() {}

void MimeHandlerViewContainer::DidFinishLoading() {
  DCHECK_NE(element_instance_id(), guestview::kInstanceIDNone);
  render_frame()->Send(new ExtensionHostMsg_CreateMimeHandlerViewGuest(
      routing_id(), html_string_, mime_type_, element_instance_id()));
}

void MimeHandlerViewContainer::DidReceiveData(const char* data,
                                              int data_length) {
  std::string value(data, data_length);
  html_string_ += value;
}

bool MimeHandlerViewContainer::HandlesMessage(const IPC::Message& message) {
  return message.type() == ExtensionMsg_CreateMimeHandlerViewGuestACK::ID;
}

bool MimeHandlerViewContainer::OnMessage(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MimeHandlerViewContainer, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_CreateMimeHandlerViewGuestACK,
                        OnCreateMimeHandlerViewGuestACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MimeHandlerViewContainer::OnCreateMimeHandlerViewGuestACK(
    int element_instance_id) {
  DCHECK_NE(this->element_instance_id(), guestview::kInstanceIDNone);
  DCHECK_EQ(this->element_instance_id(), element_instance_id);
  render_frame()->AttachGuest(element_instance_id);
}

}  // namespace extensions
