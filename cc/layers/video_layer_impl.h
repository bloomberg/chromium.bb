// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIDEO_LAYER_IMPL_H_
#define CC_LAYERS_VIDEO_LAYER_IMPL_H_

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/video_frame_provider.h"
#include "media/base/video_frame.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"

namespace media {
class SkCanvasVideoRenderer;
}

namespace cc {
class LayerTreeHostImpl;
class VideoFrameProviderClientImpl;

class CC_EXPORT VideoLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<VideoLayerImpl> Create(LayerTreeImpl* tree_impl,
                                           int id,
                                           VideoFrameProvider* provider);
  virtual ~VideoLayerImpl();

  // LayerImpl implementation.
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void WillDraw(ResourceProvider* resource_provider) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;
  virtual void DidDraw(ResourceProvider* resource_provider) OVERRIDE;
  virtual void DidBecomeActive() OVERRIDE;
  virtual void DidLoseOutputSurface() OVERRIDE;

  void SetNeedsRedraw();

  void SetProviderClientImpl(
      scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl);

  struct FramePlane {
    ResourceProvider::ResourceId resource_id;
    gfx::Size size;
    GLenum format;

    FramePlane() : resource_id(0), format(GL_LUMINANCE) {}

    bool AllocateData(ResourceProvider* resource_provider);
    void FreeData(ResourceProvider* resource_provider);
  };

 private:
  VideoLayerImpl(LayerTreeImpl* tree_impl, int id);

  virtual const char* LayerTypeAsString() const OVERRIDE;

  void WillDrawInternal(ResourceProvider* resource_provider);
  bool SetupFramePlanes(ResourceProvider* resource_provider);
  void FreeFramePlanes(ResourceProvider* resource_provider);
  void FreeUnusedFramePlanes(ResourceProvider* resource_provider);
  size_t NumPlanes() const;

  scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl_;

  scoped_refptr<media::VideoFrame> frame_;
  media::VideoFrame::Format format_;
  bool convert_yuv_;
  ResourceProvider::ResourceId external_texture_resource_;
  scoped_ptr<media::SkCanvasVideoRenderer> video_renderer_;

  // Each index in this array corresponds to a plane in media::VideoFrame.
  FramePlane frame_planes_[media::VideoFrame::kMaxPlanes];

  DISALLOW_COPY_AND_ASSIGN(VideoLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_VIDEO_LAYER_IMPL_H_
