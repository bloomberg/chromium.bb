// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_
#define CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/resources/texture_mailbox.h"

namespace media {
class SkCanvasVideoRenderer;
class VideoFrame;
}

namespace cc {
class ResourceProvider;

class VideoFrameExternalResources {
 public:
  // Specifies what type of data is contained in the mailboxes, as well as how
  // many mailboxes will be present.
  enum ResourceType {
    NONE,
    YUV_RESOURCE,
    RGB_RESOURCE,
    STREAM_TEXTURE_RESOURCE,
    IO_SURFACE,

#if defined(GOOGLE_TV)
    // TODO(danakj): Implement this with a solid color layer instead of a video
    // frame and video layer.
    HOLE,
#endif

    // TODO(danakj): Remove this and abstract TextureMailbox into
    // "ExternalResource" that can hold a hardware or software backing.
    SOFTWARE_RESOURCE
  };

  ResourceType type;
  std::vector<TextureMailbox> mailboxes;

  // TODO(danakj): Remove these too.
  std::vector<unsigned> software_resources;
  TextureMailbox::ReleaseCallback software_release_callback;

  VideoFrameExternalResources();
  ~VideoFrameExternalResources();
};

// VideoResourceUpdater is by the video system to produce frame content as
// resources consumable by the compositor.
class VideoResourceUpdater {
 public:
  explicit VideoResourceUpdater(ResourceProvider* resource_provider);
  ~VideoResourceUpdater();

  VideoFrameExternalResources CreateForHardwarePlanes(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const TextureMailbox::ReleaseCallback& release_callback);

  VideoFrameExternalResources CreateForSoftwarePlanes(
      const scoped_refptr<media::VideoFrame>& video_frame);

 private:
  bool VerifyFrame(const scoped_refptr<media::VideoFrame>& video_frame);

  static void ReturnTexture(ResourceProvider* resource_provider,
                            TextureMailbox::ReleaseCallback callback,
                            unsigned texture_id,
                            gpu::Mailbox mailbox,
                            unsigned sync_point);

  ResourceProvider* resource_provider_;
  scoped_ptr<media::SkCanvasVideoRenderer> video_renderer_;

  DISALLOW_COPY_AND_ASSIGN(VideoResourceUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_
