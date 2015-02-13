// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/browser_plugin_delegate.h"
#include "ipc/ipc_message.h"

namespace extensions {

class GuestViewContainer : public content::BrowserPluginDelegate {
 public:
  explicit GuestViewContainer(content::RenderFrame* render_frame);
  ~GuestViewContainer() override;

  // Queries whether GuestViewContainer is interested in the |message|.
  static bool HandlesMessage(const IPC::Message& message);

  void RenderFrameDestroyed();

  // BrowserPluginDelegate implementation.
  void SetElementInstanceID(int element_instance_id) override;

  int element_instance_id() const { return element_instance_id_; }
  content::RenderFrame* render_frame() const { return render_frame_; }

  virtual void OnRenderFrameDestroyed() {}

 private:
  class RenderFrameLifetimeObserver;

  int element_instance_id_;
  content::RenderFrame* render_frame_;
  scoped_ptr<RenderFrameLifetimeObserver> render_frame_lifetime_observer_;

  DISALLOW_COPY_AND_ASSIGN(GuestViewContainer);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_
