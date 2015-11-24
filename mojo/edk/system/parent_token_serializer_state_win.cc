// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/parent_token_serializer_state_win.h"

#include "base/rand_util.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_pair.h"

namespace mojo {
namespace edk {

ParentTokenSerializerState* ParentTokenSerializerState::GetInstance() {
  return base::Singleton<
      ParentTokenSerializerState,
      base::LeakySingletonTraits<ParentTokenSerializerState>>::get();
}

void ParentTokenSerializerState::CreatePlatformChannelPair(
    ScopedPlatformHandle* server, ScopedPlatformHandle* client) {
  PlatformChannelPair channel_pair;
  *server = channel_pair.PassServerHandle();
  *client = channel_pair.PassClientHandle();
}

void ParentTokenSerializerState::HandleToToken(
    const PlatformHandle* platform_handles,
    size_t count,
    uint64_t* tokens) {
  base::AutoLock auto_locker(lock_);
  for (size_t i = 0; i < count; ++i) {
    if (platform_handles[i].is_valid()) {
      uint64_t token;
      do {
        token = base::RandUint64();
      } while (!token || token_map_.find(token) != token_map_.end());
      tokens[i] = token;
      token_map_[tokens[i]] = platform_handles[i].handle;
    } else {
      DLOG(WARNING) << "ParentTokenSerializerState got invalid handle.";
      tokens[i] = 0;
    }
  }
}

void ParentTokenSerializerState::TokenToHandle(
    const uint64_t* tokens,
    size_t count,
    PlatformHandle* handles) {
  base::AutoLock auto_locker(lock_);
  for (size_t i = 0; i < count; ++i) {
    auto it = token_map_.find(tokens[i]);
    if (it == token_map_.end()) {
      DLOG(WARNING) << "TokenToHandle didn't find token.";
    } else {
      handles[i].handle = it->second;
      token_map_.erase(it);
    }
  }
}

ParentTokenSerializerState::ParentTokenSerializerState()
    : token_serialize_thread_("Token Serializer Watcher") {
  base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
  token_serialize_thread_.StartWithOptions(options);
  DCHECK(!internal::g_token_serializer);
  internal::g_token_serializer = this;
}

ParentTokenSerializerState::~ParentTokenSerializerState() {
}

}  // namespace edk
}  // namespace mojo
