// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GL_SCOPED_BINDERS_H_
#define CONTENT_COMMON_GPU_GL_SCOPED_BINDERS_H_

namespace content {

class ScopedFrameBufferBinder {
 public:
  explicit ScopedFrameBufferBinder(unsigned int fbo);
  ~ScopedFrameBufferBinder();

 private:
  int old_fbo_;
};

class ScopedTextureBinder {
 public:
  explicit ScopedTextureBinder(unsigned int id);
  ~ScopedTextureBinder();

 private:
  int old_id_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GL_SCOPED_BINDERS_H_
