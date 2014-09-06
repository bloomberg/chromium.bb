// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/browser_plugin/chrome_browser_plugin_delegate.h"

#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_messages.h"

ChromeBrowserPluginDelegate::ChromeBrowserPluginDelegate(
    content::RenderFrame* render_frame,
    const std::string& mime_type)
    : content::BrowserPluginDelegate(render_frame, mime_type),
      content::RenderFrameObserver(render_frame),
      mime_type_(mime_type),
      // TODO(lazyboy): Use browser_plugin::kInstanceIDNone.
      element_instance_id_(0) {
}

ChromeBrowserPluginDelegate::~ChromeBrowserPluginDelegate() {
}

void ChromeBrowserPluginDelegate::SetElementInstanceID(
    int element_instance_id) {
  element_instance_id_ = element_instance_id;
}

void ChromeBrowserPluginDelegate::DidFinishLoading() {
  DCHECK_NE(element_instance_id_, 0);
  render_frame()->Send(new ExtensionHostMsg_CreateMimeHandlerViewGuest(
      routing_id(), html_string_, mime_type_, element_instance_id_));
}

void ChromeBrowserPluginDelegate::DidReceiveData(const char* data,
                                                 int data_length) {
  std::string value(data, data_length);
  html_string_ += value;
}

bool ChromeBrowserPluginDelegate::OnMessageReceived(
    const IPC::Message& message) {
  if (message.type() != ExtensionMsg_CreateMimeHandlerViewGuestACK::ID)
    return false;

  DCHECK_NE(element_instance_id_, 0);
  int element_instance_id = 0;
  PickleIterator iter(message);
  bool success = iter.ReadInt(&element_instance_id);
  DCHECK(success);
  if (element_instance_id != element_instance_id_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeBrowserPluginDelegate, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_CreateMimeHandlerViewGuestACK,
                        OnCreateMimeHandlerViewGuestACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChromeBrowserPluginDelegate::OnCreateMimeHandlerViewGuestACK(
    int element_instance_id) {
  DCHECK_NE(element_instance_id_, 0);
  DCHECK_EQ(element_instance_id_, element_instance_id);
  render_frame()->AttachGuest(element_instance_id);
}
