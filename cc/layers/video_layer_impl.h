// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIDEO_LAYER_IMPL_H_
#define CC_LAYERS_VIDEO_LAYER_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/resources/video_resource_updater.h"
#include "components/viz/common/resources/release_callback.h"
#include "media/base/video_rotation.h"

namespace media {
class VideoFrame;
}

namespace cc {
class VideoFrameProvider;
class VideoFrameProviderClientImpl;

class CC_EXPORT VideoLayerImpl : public LayerImpl {
 public:
  // Must be called on the impl thread while the main thread is blocked. This is
  // so that |provider| stays alive while this is being created.
  static std::unique_ptr<VideoLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      VideoFrameProvider* provider,
      media::VideoRotation video_rotation);
  ~VideoLayerImpl() override;

  // LayerImpl implementation.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  bool WillDraw(DrawMode draw_mode,
                LayerTreeResourceProvider* resource_provider) override;
  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  void DidDraw(LayerTreeResourceProvider* resource_provider) override;
  SimpleEnclosedRegion VisibleOpaqueRegion() const override;
  void DidBecomeActive() override;
  void ReleaseResources() override;

  void SetNeedsRedraw();
  media::VideoRotation video_rotation() const { return video_rotation_; }

 private:
  VideoLayerImpl(
      LayerTreeImpl* tree_impl,
      int id,
      scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl,
      media::VideoRotation video_rotation);

  const char* LayerTypeAsString() const override;

  scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl_;

  scoped_refptr<media::VideoFrame> frame_;

  media::VideoRotation video_rotation_;

  std::unique_ptr<VideoResourceUpdater> updater_;
  VideoFrameExternalResources::ResourceType frame_resource_type_;
  float frame_resource_offset_;
  float frame_resource_multiplier_;
  uint32_t frame_bits_per_channel_;

  struct FrameResource {
    FrameResource(viz::ResourceId id,
                  gfx::Size size_in_pixels,
                  bool is_overlay_candidate)
        : id(id),
          size_in_pixels(size_in_pixels),
          is_overlay_candidate(is_overlay_candidate) {}
    viz::ResourceId id;
    gfx::Size size_in_pixels;
    bool is_overlay_candidate;
  };
  std::vector<FrameResource> frame_resources_;

  // TODO(danakj): Remove these, hide software path inside ResourceProvider and
  // ExternalResource (aka TextureMailbox) classes.
  std::vector<unsigned> software_resources_;
  // Called once for each software resource.
  viz::ReleaseCallback software_release_callback_;

  DISALLOW_COPY_AND_ASSIGN(VideoLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_VIDEO_LAYER_IMPL_H_
