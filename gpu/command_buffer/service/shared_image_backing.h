// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_

#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class MailboxManager;

// Represents the actual storage (GL texture, VkImage, GMB) for a SharedImage.
// Should not be accessed direclty, instead is accessed through a
// SharedImageRepresentation.
// TODO(ericrk): Add SharedImageRepresentation and Begin/End logic.
class SharedImageBacking {
 public:
  SharedImageBacking(viz::ResourceFormat format,
                     const gfx::Size& size,
                     const gfx::ColorSpace& color_space,
                     uint32_t usage)
      : format_(format),
        size_(size),
        color_space_(color_space),
        usage_(usage) {}

  virtual ~SharedImageBacking() {}

  viz::ResourceFormat format() const { return format_; }
  const gfx::Size& size() const { return size_; }
  const gfx::ColorSpace& color_space() const { return color_space_; }
  uint32_t usage() const { return usage_; }

  // Prepares the backing for use with the legacy mailbox system.
  // TODO(ericrk): Remove this once the new codepath is complete.
  virtual bool ProduceLegacyMailbox(const Mailbox& mailbox,
                                    MailboxManager* mailbox_manager) = 0;
  virtual void Destroy(bool have_context) = 0;

 private:
  viz::ResourceFormat format_;
  gfx::Size size_;
  gfx::ColorSpace color_space_;
  uint32_t usage_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_
