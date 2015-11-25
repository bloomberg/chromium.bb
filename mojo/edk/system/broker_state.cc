// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/broker_state.h"

#include "base/rand_util.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_pair.h"

namespace mojo {
namespace edk {

BrokerState* BrokerState::GetInstance() {
  return base::Singleton<
      BrokerState, base::LeakySingletonTraits<BrokerState>>::get();
}

#if defined(OS_WIN)
void BrokerState::CreatePlatformChannelPair(
    ScopedPlatformHandle* server, ScopedPlatformHandle* client) {
  PlatformChannelPair channel_pair;
  *server = channel_pair.PassServerHandle();
  *client = channel_pair.PassClientHandle();
}

void BrokerState::HandleToToken(
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
      DLOG(WARNING) << "BrokerState got invalid handle.";
      tokens[i] = 0;
    }
  }
}

void BrokerState::TokenToHandle(const uint64_t* tokens,
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
#endif

BrokerState::BrokerState() : broker_thread_("Mojo Broker Thread") {
  base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
  broker_thread_.StartWithOptions(options);
  DCHECK(!internal::g_broker);
  internal::g_broker = this;
}

BrokerState::~BrokerState() {
}

}  // namespace edk
}  // namespace mojo
