// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SYNC_TOKEN_H_
#define GPU_COMMAND_BUFFER_COMMON_SYNC_TOKEN_H_

#include <stdint.h>
#include <string.h>

#include "gpu/command_buffer/common/constants.h"
#include "gpu/gpu_export.h"

// From glextchromium.h.
#ifndef GL_SYNC_TOKEN_SIZE_CHROMIUM
#define GL_SYNC_TOKEN_SIZE_CHROMIUM 24
#endif

namespace gpu {

// A Sync Token is a binary blob which represents a waitable fence sync
// on a particular command buffer namespace and id.
// See src/gpu/GLES2/extensions/CHROMIUM/CHROMIUM_sync_point.txt for more
// details.
struct GPU_EXPORT SyncToken {
  SyncToken()
      : namespace_id_(CommandBufferNamespace::INVALID),
        command_buffer_id_(0),
        release_count_(0) {}

  SyncToken(CommandBufferNamespace namespace_id,
            uint64_t command_buffer_id,
            uint64_t release_count)
      : namespace_id_(namespace_id),
        command_buffer_id_(command_buffer_id),
        release_count_(release_count) {}

  void Set(CommandBufferNamespace namespace_id,
           uint64_t command_buffer_id,
           uint64_t release_count) {
    namespace_id_ = namespace_id;
    command_buffer_id_ = command_buffer_id;
    release_count_ = release_count;
  }

  int8_t* GetData() { return reinterpret_cast<int8_t*>(this); }

  const int8_t* GetConstData() const {
    return reinterpret_cast<const int8_t*>(this);
  }

  CommandBufferNamespace namespace_id() const { return namespace_id_; }
  uint64_t command_buffer_id() const { return command_buffer_id_; }
  uint64_t release_count() const { return release_count_; }

  bool operator<(const SyncToken& other) const {
    // TODO(dyen): Once all our compilers support c++11, we can replace this
    // long list of comparisons with std::tie().
    return (namespace_id_ < other.namespace_id()) ||
           ((namespace_id_ == other.namespace_id()) &&
            ((command_buffer_id_ < other.command_buffer_id()) ||
             ((command_buffer_id_ == other.command_buffer_id()) &&
              (release_count_ < other.release_count()))));
  }

 private:
  CommandBufferNamespace namespace_id_;
  uint64_t command_buffer_id_;
  uint64_t release_count_;
};

static_assert(sizeof(SyncToken) <= GL_SYNC_TOKEN_SIZE_CHROMIUM,
              "SyncToken size must not exceed GL_SYNC_TOKEN_SIZE_CHROMIUM");

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SYNC_TOKEN_H_
