// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_utils.h"

#include <va/va.h>

#include "base/logging.h"
#include "ui/gfx/geometry/size.h"

namespace media {

ScopedVABufferMapping::ScopedVABufferMapping(
    const base::Lock* lock,
    VADisplay va_display,
    VABufferID buffer_id,
    base::OnceCallback<void(VABufferID)> release_callback)
    : lock_(lock), va_display_(va_display), buffer_id_(buffer_id) {
  DCHECK(lock_);
  lock_->AssertAcquired();
  DCHECK_NE(buffer_id, VA_INVALID_ID);

  const VAStatus result =
      vaMapBuffer(va_display_, buffer_id_, &va_buffer_data_);
  const bool success = result == VA_STATUS_SUCCESS;
  LOG_IF(ERROR, !success) << "vaMapBuffer failed: " << vaErrorStr(result);
  DCHECK(success == (va_buffer_data_ != nullptr))
      << "|va_buffer_data| should be null if vaMapBuffer() fails";

  if (!success && release_callback)
    std::move(release_callback).Run(buffer_id_);
}

ScopedVABufferMapping::~ScopedVABufferMapping() {
  lock_->AssertAcquired();
  if (va_buffer_data_)
    Unmap();
}

VAStatus ScopedVABufferMapping::Unmap() {
  lock_->AssertAcquired();
  const VAStatus result = vaUnmapBuffer(va_display_, buffer_id_);
  if (result == VA_STATUS_SUCCESS)
    va_buffer_data_ = nullptr;
  else
    LOG(ERROR) << "vaUnmapBuffer failed: " << vaErrorStr(result);
  return result;
}

ScopedVAImage::ScopedVAImage(base::Lock* lock,
                             VADisplay va_display,
                             VASurfaceID va_surface_id,
                             VAImageFormat* format,
                             const gfx::Size& size)
    : lock_(lock), va_display_(va_display), image_(new VAImage{}) {
  DCHECK(lock_);
  lock_->AssertAcquired();
  VAStatus result = vaCreateImage(va_display_, format, size.width(),
                                  size.height(), image_.get());
  if (result != VA_STATUS_SUCCESS) {
    DCHECK_EQ(image_->image_id, VA_INVALID_ID);
    LOG(ERROR) << "vaCreateImage failed: " << vaErrorStr(result);
    return;
  }

  result = vaGetImage(va_display_, va_surface_id, 0, 0, size.width(),
                      size.height(), image_->image_id);
  if (result != VA_STATUS_SUCCESS) {
    LOG(ERROR) << "vaGetImage failed: " << vaErrorStr(result);
    return;
  }

  va_buffer_ =
      std::make_unique<ScopedVABufferMapping>(lock_, va_display, image_->buf);
}

ScopedVAImage::~ScopedVAImage() {
  base::AutoLock auto_lock(*lock_);

  // |va_buffer_| has to be deleted before vaDestroyImage().
  va_buffer_.release();
  vaDestroyImage(va_display_, image_->image_id);
}

}  // namespace media
