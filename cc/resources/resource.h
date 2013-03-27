// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_H_
#define CC_RESOURCES_RESOURCE_H_

#include "cc/base/cc_export.h"
#include "cc/resources/resource_provider.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT Resource {
 public:
  Resource() : id_(0) {}
  Resource(unsigned id, gfx::Size size, GLenum format)
      : id_(id),
        size_(size),
        format_(format) {}

  ResourceProvider::ResourceId id() const { return id_; }
  const gfx::Size& size() const { return size_; }
  GLenum format() const { return format_; }

  size_t bytes() const;

  static size_t BytesPerPixel(GLenum format);
  static size_t MemorySizeBytes(const gfx::Size& size, GLenum format);

 protected:
  void set_id(ResourceProvider::ResourceId id) { id_ = id; }
  void set_dimensions(const gfx::Size&, GLenum format);

 private:
  ResourceProvider::ResourceId id_;
  gfx::Size size_;
  GLenum format_;

  DISALLOW_COPY_AND_ASSIGN(Resource);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_H_
