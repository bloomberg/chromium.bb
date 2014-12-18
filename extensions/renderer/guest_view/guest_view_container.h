// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_

#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/render_frame_observer.h"
#include "ipc/ipc_message.h"

namespace extensions {

class GuestViewContainer : public content::BrowserPluginDelegate {
 public:
  explicit GuestViewContainer(content::RenderFrame* render_frame);
  ~GuestViewContainer() override;

  // Queries whether GuestViewContainer is interested in the |message|.
  static bool HandlesMessage(const IPC::Message& message);

  // BrowserPluginDelegate implementation.
  void SetElementInstanceID(int element_instance_id) override;

  int element_instance_id() const { return element_instance_id_; }
  int render_view_routing_id() const { return render_view_routing_id_; }
  content::RenderFrame* render_frame() const { return render_frame_; }

 private:
  int element_instance_id_;
  const int render_view_routing_id_;
  content::RenderFrame* const render_frame_;

  DISALLOW_COPY_AND_ASSIGN(GuestViewContainer);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_
