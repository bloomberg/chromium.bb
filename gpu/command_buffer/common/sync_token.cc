// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/sync_token.h"

#include <string.h>

#include "base/logging.h"
#include "base/rand_util.h"

namespace gpu {

namespace {

struct SyncTokenInternal {
  CommandBufferNamespace namespace_id;
  uint64_t command_buffer_id;
  uint64_t release_count;

  bool operator<(const SyncTokenInternal& other) const {
    // TODO(dyen): Once all our compilers support c++11, we can replace this
    // long list of comparisons with std::tie().
    return (namespace_id < other.namespace_id) ||
           ((namespace_id == other.namespace_id) &&
            ((command_buffer_id < other.command_buffer_id) ||
             ((command_buffer_id == other.command_buffer_id) &&
              (release_count < other.release_count))));
  }
};

static_assert(sizeof(SyncTokenInternal) <= GL_SYNC_TOKEN_SIZE_CHROMIUM,
              "SyncTokenInternal must not exceed GL_SYNC_TOKEN_SIZE_CHROMIUM");
static_assert(sizeof(SyncToken) == GL_SYNC_TOKEN_SIZE_CHROMIUM,
              "No additional members other than data are allowed in SyncToken");

}  // namespace

SyncToken::SyncToken() {
  memset(data, 0, sizeof(data));
}

SyncToken::SyncToken(CommandBufferNamespace namespace_id,
                     uint64_t command_buffer_id,
                     uint64_t release_count) {
  memset(data, 0, sizeof(data));
  SetData(namespace_id, command_buffer_id, release_count);
}

CommandBufferNamespace SyncToken::GetNamespaceId() const {
  return reinterpret_cast<const SyncTokenInternal*>(data)->namespace_id;
}

uint64_t SyncToken::GetCommandBufferId() const {
  return reinterpret_cast<const SyncTokenInternal*>(data)->command_buffer_id;
}

uint64_t SyncToken::GetReleaseCount() const {
  return reinterpret_cast<const SyncTokenInternal*>(data)->release_count;
}

void SyncToken::SetData(CommandBufferNamespace namespace_id,
                        uint64_t command_buffer_id,
                        uint64_t release_count) {
  SyncTokenInternal* sync_token_internal =
      reinterpret_cast<SyncTokenInternal*>(data);
  sync_token_internal->namespace_id = namespace_id;
  sync_token_internal->command_buffer_id = command_buffer_id;
  sync_token_internal->release_count = release_count;
}

bool SyncToken::operator<(const SyncToken& other) const {
  const SyncTokenInternal* sync_token_internal =
      reinterpret_cast<const SyncTokenInternal*>(data);
  const SyncTokenInternal* other_sync_token_internal =
      reinterpret_cast<const SyncTokenInternal*>(other.data);

  return *sync_token_internal < *other_sync_token_internal;
}

}  // namespace gpu
