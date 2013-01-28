// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_SHARED_BUFFER_FACTORY_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_SHARED_BUFFER_FACTORY_H_

namespace media {

class SharedBuffer;

// Provides a way to create shared buffers accessible by two or more processes.
class SharedBufferFactory {
 public:
  // Creates a shared memory buffer of the given size.
  virtual scoped_refptr<SharedBuffer> CreateSharedBuffer(uint32 size) = 0;

  // Notifies the factory that the buffer is no longer used by the caller and
  // can be released. The caller still has to drop all references to free
  // the memory.
  virtual void ReleaseSharedBuffer(scoped_refptr<SharedBuffer> buffer) = 0;

 protected:
  virtual ~SharedBufferFactory() {}
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_SHARED_BUFFER_FACTORY_H_
