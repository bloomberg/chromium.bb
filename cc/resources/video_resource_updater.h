// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_
#define CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/cc_export.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class PaintCanvasVideoRenderer;
class VideoFrame;
}

namespace gfx {
class Rect;
class Transform;
}  // namespace gfx

namespace viz {
class ContextProvider;
class RenderPass;
}

namespace cc {
class LayerTreeFrameSink;
class LayerTreeResourceProvider;

// Specifies what type of data is contained in the mailboxes, as well as how
// many mailboxes will be present.
enum class VideoFrameResourceType {
  NONE,
  YUV,
  RGB,
  RGBA_PREMULTIPLIED,
  RGBA,
  STREAM_TEXTURE,
};

class CC_EXPORT VideoFrameExternalResources {
 public:
  VideoFrameResourceType type = VideoFrameResourceType::NONE;
  std::vector<viz::TransferableResource> resources;
  std::vector<viz::ReleaseCallback> release_callbacks;

  // Used by hardware textures which do not return values in the 0-1 range.
  // After a lookup, subtract offset and multiply by multiplier.
  float offset = 0.f;
  float multiplier = 1.f;
  uint32_t bits_per_channel = 8;

  VideoFrameExternalResources();
  VideoFrameExternalResources(VideoFrameExternalResources&& other);
  VideoFrameExternalResources& operator=(VideoFrameExternalResources&& other);
  ~VideoFrameExternalResources();
};

// VideoResourceUpdater is used by the video system to produce frame content as
// resources consumable by the compositor.
class CC_EXPORT VideoResourceUpdater
    : public base::trace_event::MemoryDumpProvider {
 public:
  // For GPU compositing |context_provider| should be provided and for software
  // compositing |layer_tree_frame_sink| should be provided. If there is a
  // non-null |context_provider| we assume GPU compositing.
  // TODO(kylechar): Don't use LayerTreeFrameSink for the software compositing
  // path. The UseSurfaceLayerForVideo path isn't compatible with this. We can
  // maybe use mojom::CompositorFrameSink instead.
  VideoResourceUpdater(viz::ContextProvider* context_provider,
                       LayerTreeFrameSink* layer_tree_frame_sink,
                       LayerTreeResourceProvider* resource_provider,
                       bool use_stream_video_draw_quad,
                       bool use_gpu_memory_buffer_resources);

  ~VideoResourceUpdater() override;

  // For each CompositorFrame the following sequence is expected:
  // 1. ObtainFrameResources(): Import resources for the next video frame with
  //    LayerTreeResourceProvider. This will reuse existing GPU or SharedBitmap
  //    buffers if possible, otherwise it will allocate new ones.
  // 2. AppendQuads(): Add DrawQuads to CompositorFrame for video.
  // 3. ReleaseFrameResources(): After the CompositorFrame has been submitted,
  //    remove imported resources from LayerTreeResourceProvider.
  void ObtainFrameResources(scoped_refptr<media::VideoFrame> video_frame);
  void ReleaseFrameResources();
  void AppendQuads(viz::RenderPass* render_pass,
                   scoped_refptr<media::VideoFrame> frame,
                   gfx::Transform transform,
                   gfx::Size rotated_size,
                   gfx::Rect visible_layer_rect,
                   gfx::Rect clip_rect,
                   bool is_clipped,
                   bool context_opaque,
                   float draw_opacity,
                   int sorting_context_id,
                   gfx::Rect visible_quad_rect);

  // TODO(kylechar): This is only public for testing, make private.
  VideoFrameExternalResources CreateExternalResourcesFromVideoFrame(
      scoped_refptr<media::VideoFrame> video_frame);

 private:
  class PlaneResource;
  class HardwarePlaneResource;
  class SoftwarePlaneResource;

  // A resource that will be embedded in a DrawQuad in the next CompositorFrame.
  // Each video plane will correspond to one FrameResource.
  struct FrameResource {
    viz::ResourceId id;
    gfx::Size size_in_pixels;
  };

  bool software_compositor() const { return context_provider_ == nullptr; }

  // Obtain a resource of the right format by either recycling an
  // unreferenced but appropriately formatted resource, or by
  // allocating a new resource.
  // Additionally, if the |unique_id| and |plane_index| match, then
  // it is assumed that the resource has the right data already and will only be
  // used for reading, and so is returned even if it is still referenced.
  // Passing -1 for |plane_index| avoids returning referenced
  // resources.
  PlaneResource* RecycleOrAllocateResource(const gfx::Size& resource_size,
                                           viz::ResourceFormat resource_format,
                                           const gfx::ColorSpace& color_space,
                                           int unique_id,
                                           int plane_index);
  PlaneResource* AllocateResource(const gfx::Size& plane_size,
                                  viz::ResourceFormat format,
                                  const gfx::ColorSpace& color_space);

  // Create a copy of a texture-backed source video frame in a new GL_TEXTURE_2D
  // texture. This is used when there are multiple GPU threads (Android WebView)
  // and the source video frame texture can't be used on the output GL context.
  // https://crbug.com/582170
  void CopyHardwarePlane(media::VideoFrame* video_frame,
                         const gfx::ColorSpace& resource_color_space,
                         const gpu::MailboxHolder& mailbox_holder,
                         VideoFrameExternalResources* external_resources);

  // Get resources ready to be appended into DrawQuads. This is used for GPU
  // compositing most of the time, except for the cases mentioned in
  // CreateForSoftwarePlanes().
  VideoFrameExternalResources CreateForHardwarePlanes(
      scoped_refptr<media::VideoFrame> video_frame);

  // Get resources ready to be appended into DrawQuads. This is always used for
  // software compositing. This is also used for GPU compositing when the input
  // video frame has no textures.
  VideoFrameExternalResources CreateForSoftwarePlanes(
      scoped_refptr<media::VideoFrame> video_frame);

  void RecycleResource(uint32_t plane_resource_id,
                       const gpu::SyncToken& sync_token,
                       bool lost_resource);
  void ReturnTexture(const scoped_refptr<media::VideoFrame>& video_frame,
                     const gpu::SyncToken& sync_token,
                     bool lost_resource);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  viz::ContextProvider* const context_provider_;
  LayerTreeFrameSink* const layer_tree_frame_sink_;
  LayerTreeResourceProvider* const resource_provider_;
  const bool use_stream_video_draw_quad_;
  const bool use_gpu_memory_buffer_resources_;
  const int tracing_id_;
  std::unique_ptr<media::PaintCanvasVideoRenderer> video_renderer_;
  uint32_t next_plane_resource_id_ = 1;

  // Temporary pixel buffer when converting between formats.
  std::vector<uint8_t> upload_pixels_;

  VideoFrameResourceType frame_resource_type_;

  float frame_resource_offset_;
  float frame_resource_multiplier_;
  uint32_t frame_bits_per_channel_;

  // Resources that will be placed into quads by the next call to
  // AppendDrawQuads().
  std::vector<FrameResource> frame_resources_;

  // Resources allocated by VideoResourceUpdater. Used to recycle resources so
  // we can reduce the number of allocations and data transfers.
  std::vector<std::unique_ptr<PlaneResource>> all_resources_;

  base::WeakPtrFactory<VideoResourceUpdater> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoResourceUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_
