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
  m->WriteUInt32(base::checked_cast<int>(u.minDeadCapacity));
  m->WriteUInt32(base::checked_cast<int>(u.maxDeadCapacity));
  m->WriteUInt32(base::checked_cast<int>(u.capacity));
  m->WriteUInt32(base::checked_cast<int>(u.liveSize));
  m->WriteUInt32(base::checked_cast<int>(u.deadSize));
}

bool ParamTraits<blink::WebCache::UsageStats>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    blink::WebCache::UsageStats* u) {
  uint32_t min_capacity, max_capacity, capacity, live_size, dead_size;
  if (!iter->ReadUInt32(&min_capacity) ||
      !iter->ReadUInt32(&max_capacity) ||
      !iter->ReadUInt32(&capacity) ||
      !iter->ReadUInt32(&live_size) ||
      !iter->ReadUInt32(&dead_size)) {
    return false;
  }

  u->minDeadCapacity = min_capacity;
  u->maxDeadCapacity = max_capacity;
  u->capacity = capacity;
  u->liveSize = live_size;
  u->deadSize = dead_size;

  return true;
}

void ParamTraits<blink::WebCache::UsageStats>::Log(
    const blink::WebCache::UsageStats& p, std::string* l) {
  l->append("<blink::WebCache::UsageStats>");
}

}  // namespace IPC
