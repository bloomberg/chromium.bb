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
class LayerTreeResourceProvider;

class CC_EXPORT VideoFrameExternalResources {
 public:
  // Specifies what type of data is contained in the mailboxes, as well as how
  // many mailboxes will be present.
  enum ResourceType {
    NONE,
    YUV_RESOURCE,
    RGB_RESOURCE,
    RGBA_PREMULTIPLIED_RESOURCE,
    RGBA_RESOURCE,
    STREAM_TEXTURE_RESOURCE,

    SOFTWARE_RESOURCE
  };
  ResourceType type = NONE;
  std::vector<viz::TransferableResource> resources;
  std::vector<viz::ReleaseCallback> release_callbacks;

  // TODO(danakj): Remove these and use TransferableResources to send
  // software things too.
  unsigned software_resource = viz::kInvalidResourceId;
  viz::ReleaseCallback software_release_callback;

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
class CC_EXPORT VideoResourceUpdater {
 public:
  VideoResourceUpdater(viz::ContextProvider* context_provider,
                       LayerTreeResourceProvider* resource_provider,
                       bool use_stream_video_draw_quad);
  ~VideoResourceUpdater();

  VideoFrameExternalResources CreateExternalResourcesFromVideoFrame(
      scoped_refptr<media::VideoFrame> video_frame);

  void SetUseR16ForTesting(bool use_r16_for_testing) {
    use_r16_for_testing_ = use_r16_for_testing;
  }

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

 private:
  class PlaneResource {
   public:
    PlaneResource(unsigned resource_id,
                  const gfx::Size& resource_size,
                  viz::ResourceFormat resource_format,
                  gpu::Mailbox mailbox);
    PlaneResource(const PlaneResource& other);

    // Returns true if this resource matches the unique identifiers of another
    // VideoFrame resource.
    bool Matches(int unique_frame_id, size_t plane_index);

    // Sets the unique identifiers for this resource, may only be called when
    // there is a single reference to the resource (i.e. |ref_count_| == 1).
    void SetUniqueId(int unique_frame_id, size_t plane_index);

    // Accessors for resource identifiers provided at construction time.
    unsigned resource_id() const { return resource_id_; }
    const gfx::Size& resource_size() const { return resource_size_; }
    viz::ResourceFormat resource_format() const { return resource_format_; }
    const gpu::Mailbox& mailbox() const { return mailbox_; }

    // Various methods for managing references. See |ref_count_| for details.
    void add_ref() { ++ref_count_; }
    void remove_ref() { --ref_count_; }
    void clear_refs() { ref_count_ = 0; }
    bool has_refs() const { return ref_count_ != 0; }

   private:
    // The balance between the number of times this resource has been returned
    // from CreateForSoftwarePlanes vs released in RecycleResource.
    int ref_count_ = 0;

    // These two members are used for identifying the data stored in this
    // resource; they uniquely identify a media::VideoFrame plane.
    int unique_frame_id_ = 0;
    size_t plane_index_ = 0u;
    // Indicates if the above two members have been set or not.
    bool has_unique_frame_id_and_plane_index_ = false;

    const unsigned resource_id_;
    const gfx::Size resource_size_;
    const viz::ResourceFormat resource_format_;
    const gpu::Mailbox mailbox_;
  };

  // This needs to be a container where iterators can be erased without
  // invalidating other iterators.
  typedef std::list<PlaneResource> ResourceList;

  // Obtain a resource of the right format by either recycling an
  // unreferenced but appropriately formatted resource, or by
  // allocating a new resource.
  // Additionally, if the |unique_id| and |plane_index| match, then
  // it is assumed that the resource has the right data already and will only be
  // used for reading, and so is returned even if it is still referenced.
  // Passing -1 for |plane_index| avoids returning referenced
  // resources.
  ResourceList::iterator RecycleOrAllocateResource(
      const gfx::Size& resource_size,
      viz::ResourceFormat resource_format,
      const gfx::ColorSpace& color_space,
      bool software_resource,
      int unique_id,
      int plane_index);
  ResourceList::iterator AllocateResource(const gfx::Size& plane_size,
                                          viz::ResourceFormat format,
                                          const gfx::ColorSpace& color_space,
                                          bool software_resource);
  void DeleteResource(ResourceList::iterator resource_it);
  void CopyHardwarePlane(media::VideoFrame* video_frame,
                         const gfx::ColorSpace& resource_color_space,
                         const gpu::MailboxHolder& mailbox_holder,
                         VideoFrameExternalResources* external_resources);
  VideoFrameExternalResources CreateForHardwarePlanes(
      scoped_refptr<media::VideoFrame> video_frame);
  VideoFrameExternalResources CreateForSoftwarePlanes(
      scoped_refptr<media::VideoFrame> video_frame);

  static void RecycleResource(base::WeakPtr<VideoResourceUpdater> updater,
                              unsigned resource_id,
                              const gpu::SyncToken& sync_token,
                              bool lost_resource);
  static void ReturnTexture(base::WeakPtr<VideoResourceUpdater> updater,
                            const scoped_refptr<media::VideoFrame>& video_frame,
                            const gpu::SyncToken& sync_token,
                            bool lost_resource);

  viz::ContextProvider* context_provider_;
  LayerTreeResourceProvider* resource_provider_;
  const bool use_stream_video_draw_quad_;
  std::unique_ptr<media::PaintCanvasVideoRenderer> video_renderer_;
  std::vector<uint8_t> upload_pixels_;
  bool use_r16_for_testing_ = false;

  VideoFrameExternalResources::ResourceType frame_resource_type_;
  // TODO(danakj): Remove these, use TransferableResource for software path too.
  unsigned software_resource_ = viz::kInvalidResourceId;
  // Called once for |software_resource_|.
  viz::ReleaseCallback software_release_callback_;

  float frame_resource_offset_;
  float frame_resource_multiplier_;
  uint32_t frame_bits_per_channel_;

  struct FrameResource {
    FrameResource(viz::ResourceId id, gfx::Size size_in_pixels)
        : id(id), size_in_pixels(size_in_pixels) {}
    viz::ResourceId id;
    gfx::Size size_in_pixels;
  };
  std::vector<FrameResource> frame_resources_;

  // Recycle resources so that we can reduce the number of allocations and
  // data transfers.
  ResourceList all_resources_;

  base::WeakPtrFactory<VideoResourceUpdater> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoResourceUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_
