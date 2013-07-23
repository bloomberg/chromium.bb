/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <assert.h>

#include "nacl_io/event_emitter.h"
#include "nacl_io/event_listener.h"

#include "sdk_util/auto_lock.h"

namespace nacl_io {

bool operator<(const ScopedEventInfo& src_a, const ScopedEventInfo& src_b) {
  return src_a.get() < src_b.get();
}

void EventEmitter::Destroy() {
  // We can not grab the EmitterLock prior to grabbing the EventListener lock,
  // however the ref count proves this is the only thread which has a
  // reference to the emitter at this point so accessing events_ is safe.
  EventInfoSet_t::iterator it;
  for (it = events_.begin(); it != events_.end(); it++) {
    ScopedEventInfo info = *it;
    info->listener->AbandonedEventInfo(info);
  }
}

void EventEmitter::RegisterEventInfo(const ScopedEventInfo& info) {
  AUTO_LOCK(emitter_lock_);

  events_.insert(info);
  ChainRegisterEventInfo(info);
}

void EventEmitter::UnregisterEventInfo(const ScopedEventInfo& info) {
  AUTO_LOCK(emitter_lock_);

  ChainUnregisterEventInfo(info);
  events_.erase(info);
}
void EventEmitter::RaiseEvent(uint32_t event_bits) {
  AUTO_LOCK(emitter_lock_);

  EventInfoSet_t::iterator it;
  for (it = events_.begin(); it != events_.end(); it++) {
    // If this event is allowed by the filter, signal it
    ScopedEventInfo info = *it;
    if (info->filter & event_bits) {
      info->events |= event_bits & info->filter;
      info->listener->Signal(info);
    }
  }
}

void EventEmitter::ChainRegisterEventInfo(const ScopedEventInfo& info) {}
void EventEmitter::ChainUnregisterEventInfo(const ScopedEventInfo& info) {}

}  // namespace nacl_io
