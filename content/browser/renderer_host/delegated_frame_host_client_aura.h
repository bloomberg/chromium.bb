// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_AURA_H_

#include "base/macros.h"
#include "content/browser/renderer_host/compositor_resize_lock.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/common/content_export.h"

namespace content {

class RenderWidgetHostViewAura;

// DelegatedFrameHostClient implementation for aura, not used in mus.
class CONTENT_EXPORT DelegatedFrameHostClientAura
    : public DelegatedFrameHostClient,
      public CompositorResizeLockClient {
 public:
  explicit DelegatedFrameHostClientAura(
      RenderWidgetHostViewAura* render_widget_host_view);
  ~DelegatedFrameHostClientAura() override;

 protected:
  RenderWidgetHostViewAura* render_widget_host_view() {
    return render_widget_host_view_;
  }

  // DelegatedFrameHostClient implementation.
  ui::Layer* DelegatedFrameHostGetLayer() const override;
  bool DelegatedFrameHostIsVisible() const override;
  SkColor DelegatedFrameHostGetGutterColor(SkColor color) const override;
  gfx::Size DelegatedFrameHostDesiredSizeInDIP() const override;
  bool DelegatedFrameCanCreateResizeLock() const override;
  std::unique_ptr<CompositorResizeLock> DelegatedFrameHostCreateResizeLock()
      override;
  viz::LocalSurfaceId GetLocalSurfaceId() const override;
  void OnBeginFrame() override;
  bool IsAutoResizeEnabled() const override;

  // CompositorResizeLockClient implementation.
  std::unique_ptr<ui::CompositorLock> GetCompositorLock(
      ui::CompositorLockClient* client) override;
  void CompositorResizeLockEnded() override;

 private:
  RenderWidgetHostViewAura* render_widget_host_view_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedFrameHostClientAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_AURA_H_
