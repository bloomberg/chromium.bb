// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_YUV_VIDEO_DRAW_QUAD_H_
#define CC_YUV_VIDEO_DRAW_QUAD_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/draw_quad.h"
#include "cc/video_layer_impl.h"

namespace cc {

class CC_EXPORT YUVVideoDrawQuad : public DrawQuad {
 public:
  virtual ~YUVVideoDrawQuad();

  static scoped_ptr<YUVVideoDrawQuad> Create();

  void SetNew(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::SizeF tex_scale,
              const VideoLayerImpl::FramePlane& y_plane,
              const VideoLayerImpl::FramePlane& u_plane,
              const VideoLayerImpl::FramePlane& v_plane);

  void SetAll(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::Rect visible_rect,
              bool needs_blending,
              gfx::SizeF tex_scale,
              const VideoLayerImpl::FramePlane& y_plane,
              const VideoLayerImpl::FramePlane& u_plane,
              const VideoLayerImpl::FramePlane& v_plane);

  gfx::SizeF tex_scale;
  VideoLayerImpl::FramePlane y_plane;
  VideoLayerImpl::FramePlane u_plane;
  VideoLayerImpl::FramePlane v_plane;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const YUVVideoDrawQuad* MaterialCast(const DrawQuad*);
 private:
  YUVVideoDrawQuad();
};

}

#endif  // CC_YUV_VIDEO_DRAW_QUAD_H_
