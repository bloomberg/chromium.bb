// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TEXTURE_MAILBOX_H_
#define CC_RESOURCES_TEXTURE_MAILBOX_H_

#include <string>

#include "base/callback.h"
#include "base/memory/shared_memory.h"
#include "cc/base/cc_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/size.h"

namespace cc {

// TODO(skaslev, danakj) Rename this class more apropriately since now it
// can hold a shared memory resource as well as a texture mailbox.
class CC_EXPORT TextureMailbox {
 public:
  typedef base::Callback<void(unsigned sync_point,
                              bool lost_resource)> ReleaseCallback;
  TextureMailbox();
  TextureMailbox(const std::string& mailbox_name,
                 const ReleaseCallback& callback);
  TextureMailbox(const gpu::Mailbox& mailbox_name,
                 const ReleaseCallback& callback);
  TextureMailbox(const gpu::Mailbox& mailbox_name,
                 const ReleaseCallback& callback,
                 unsigned sync_point);
  TextureMailbox(const gpu::Mailbox& mailbox_name,
                 const ReleaseCallback& callback,
                 unsigned texture_target,
                 unsigned sync_point);
  TextureMailbox(base::SharedMemory* shared_memory,
                 gfx::Size size,
                 const ReleaseCallback& callback);

  ~TextureMailbox();

  bool IsValid() const { return IsTexture() || IsSharedMemory(); }
  bool IsTexture() const { return !name_.IsZero(); }
  bool IsSharedMemory() const { return shared_memory_ != NULL; }

  bool Equals(const TextureMailbox&) const;
  bool ContainsMailbox(const gpu::Mailbox&) const;
  bool ContainsHandle(base::SharedMemoryHandle handle) const;

  const ReleaseCallback& callback() const { return callback_; }
  const int8* data() const { return name_.name; }
  const gpu::Mailbox& name() const { return name_; }
  void ResetSyncPoint() { sync_point_ = 0; }
  void RunReleaseCallback(unsigned sync_point, bool lost_resource) const;
  void SetName(const gpu::Mailbox&);
  unsigned target() const { return target_; }
  unsigned sync_point() const { return sync_point_; }

  base::SharedMemory* shared_memory() const { return shared_memory_; }
  gfx::Size shared_memory_size() const { return shared_memory_size_; }
  size_t shared_memory_size_in_bytes() const;

  TextureMailbox CopyWithNewCallback(const ReleaseCallback& callback) const;

 private:
  gpu::Mailbox name_;
  ReleaseCallback callback_;
  unsigned target_;
  unsigned sync_point_;
  base::SharedMemory* shared_memory_;
  gfx::Size shared_memory_size_;
};

}  // namespace cc

#endif  // CC_RESOURCES_TEXTURE_MAILBOX_H_
