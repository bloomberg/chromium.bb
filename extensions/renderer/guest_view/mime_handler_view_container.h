// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_

#include "extensions/renderer/guest_view/guest_view_container.h"

namespace extensions {

class MimeHandlerViewContainer : public GuestViewContainer {
 public:
  MimeHandlerViewContainer(content::RenderFrame* render_frame,
                           const std::string& mime_type);
  ~MimeHandlerViewContainer() override;

  // BrowserPluginDelegate implementation.
  void DidFinishLoading() override;
  void DidReceiveData(const char* data, int data_length) override;

  // GuestViewContainer override.
  bool HandlesMessage(const IPC::Message& message) override;
  bool OnMessage(const IPC::Message& message) override;

 private:
  void OnCreateMimeHandlerViewGuestACK(int element_instance_id);

  const std::string mime_type_;
  std::string html_string_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewContainer);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_
