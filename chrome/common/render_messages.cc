// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages.h"

#include "chrome/common/content_settings_pattern_serializer.h"

namespace IPC {

void ParamTraits<ContentSettingsPattern>::Write(
    base::Pickle* m,
    const ContentSettingsPattern& pattern) {
  ContentSettingsPatternSerializer::WriteToMessage(pattern, m);
}

bool ParamTraits<ContentSettingsPattern>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    ContentSettingsPattern* pattern) {
  return ContentSettingsPatternSerializer::ReadFromMessage(m, iter, pattern);
}

void ParamTraits<ContentSettingsPattern>::Log(
    const ContentSettingsPattern& p, std::string* l) {
  l->append("<ContentSettingsPattern: ");
  l->append(p.ToString());
  l->append(">");
}

void ParamTraits<blink::WebCache::UsageStats>::Write(
    base::Pickle* m, const blink::WebCache::UsageStats& u) {
  m->WriteUInt64(static_cast<uint64_t>(u.minDeadCapacity));
  m->WriteUInt64(static_cast<uint64_t>(u.maxDeadCapacity));
  m->WriteUInt64(static_cast<uint64_t>(u.capacity));
  m->WriteUInt64(static_cast<uint64_t>(u.liveSize));
  m->WriteUInt64(static_cast<uint64_t>(u.deadSize));
}

bool ParamTraits<blink::WebCache::UsageStats>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    blink::WebCache::UsageStats* u) {
  uint64_t min_capacity, max_capacity, capacity, live_size, dead_size;
  if (!iter->ReadUInt64(&min_capacity) ||
      !iter->ReadUInt64(&max_capacity) ||
      !iter->ReadUInt64(&capacity) ||
      !iter->ReadUInt64(&live_size) ||
      !iter->ReadUInt64(&dead_size)) {
    return false;
  }

  u->minDeadCapacity = base::checked_cast<size_t>(min_capacity);
  u->maxDeadCapacity = base::checked_cast<size_t>(max_capacity);
  u->capacity = base::checked_cast<size_t>(capacity);
  u->liveSize = base::checked_cast<size_t>(live_size);
  u->deadSize = base::checked_cast<size_t>(dead_size);

  return true;
}

void ParamTraits<blink::WebCache::UsageStats>::Log(
    const blink::WebCache::UsageStats& p, std::string* l) {
  l->append("<blink::WebCache::UsageStats>");
}

}  // namespace IPC
