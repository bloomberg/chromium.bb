// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_

#include <memory>

#include "components/viz/common/quads/texture_mailbox.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

class SkBitmap;

namespace viz {

// Base class for providing the result of a CopyOutputRequest. Implementations
// that execute CopyOutputRequests will use a subclass implementation to define
// data storage, access and ownership semantics relative to the lifetime of the
// CopyOutputResult instance.
class VIZ_COMMON_EXPORT CopyOutputResult {
 public:
  enum class Format : uint8_t {
    // A normal bitmap in system memory. AsSkBitmap() will return a bitmap in
    // "N32Premul" form.
    RGBA_BITMAP,
    // A GL_RGBA texture, referenced by a TextureMailbox. Client code can
    // optionally take ownership of the texture (via TakeTextureOwnership()), if
    // it is needed beyond the lifetime of CopyOutputResult.
    RGBA_TEXTURE,
  };

  CopyOutputResult(Format format, const gfx::Rect& rect);

  virtual ~CopyOutputResult();

  // Returns false if the request succeeded and the data accessors will return
  // valid references.
  bool IsEmpty() const;

  // Returns the format of this result.
  Format format() const { return format_; }

  // Returns the result Rect, which is the position and size of the image data
  // within the surface/layer (see CopyOutputRequest::set_area()).
  const gfx::Rect& rect() const { return rect_; }
  const gfx::Size& size() const { return rect_.size(); }

  // Convenience to provide this result in SkBitmap form. Returns a
  // !readyToDraw() bitmap if this result is empty or if a conversion is not
  // possible in the current implementation.
  virtual const SkBitmap& AsSkBitmap() const;

  // Returns a pointer to the TextureMailbox referencing a RGBA_TEXTURE result,
  // or null if this is not a RGBA_TEXTURE result. Clients can either:
  //   1. Let CopyOutputResult retain ownership and the texture will only be
  //      valid for use during CopyOutputResult's lifetime.
  //   2. Take over ownership of the texture by calling TakeTextureOwnership(),
  //      and the client must guarantee the release callback will be run at some
  //      point.
  virtual const TextureMailbox* GetTextureMailbox() const;
  virtual std::unique_ptr<SingleReleaseCallback> TakeTextureOwnership();

 protected:
  // Accessor for subclasses to initialize the cached SkBitmap.
  SkBitmap* cached_bitmap() const { return &cached_bitmap_; }

 private:
  const Format format_;
  const gfx::Rect rect_;

  // Cached bitmap returned by the default implementation of AsSkBitmap().
  mutable SkBitmap cached_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(CopyOutputResult);
};

// Subclass of CopyOutputResult that provides a RGBA_BITMAP result from an
// SkBitmap.
class VIZ_COMMON_EXPORT CopyOutputSkBitmapResult : public CopyOutputResult {
 public:
  CopyOutputSkBitmapResult(const gfx::Rect& rect, const SkBitmap& bitmap);
  ~CopyOutputSkBitmapResult() override;

  const SkBitmap& AsSkBitmap() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CopyOutputSkBitmapResult);
};

// Subclass of CopyOutputResult that holds a reference to a texture (via
// TextureMailbox) and owns the texture, calling its SingleReleaseCallback at
// destruction time.
class VIZ_COMMON_EXPORT CopyOutputTextureResult : public CopyOutputResult {
 public:
  CopyOutputTextureResult(
      const gfx::Rect& rect,
      const TextureMailbox& texture_mailbox,
      std::unique_ptr<SingleReleaseCallback> release_callback);
  ~CopyOutputTextureResult() override;

  const TextureMailbox* GetTextureMailbox() const override;
  std::unique_ptr<SingleReleaseCallback> TakeTextureOwnership() override;

 private:
  TextureMailbox texture_mailbox_;
  std::unique_ptr<SingleReleaseCallback> release_callback_;

  DISALLOW_COPY_AND_ASSIGN(CopyOutputTextureResult);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_
