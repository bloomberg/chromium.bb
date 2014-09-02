// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages.h"

#include "chrome/common/content_settings_pattern_serializer.h"

namespace IPC {

void ParamTraits<ContentSettingsPattern>::Write(
    Message* m, const ContentSettingsPattern& pattern) {
  ContentSettingsPatternSerializer::WriteToMessage(pattern, m);
}

bool ParamTraits<ContentSettingsPattern>::Read(
    const Message* m, PickleIterator* iter, ContentSettingsPattern* pattern) {
  return ContentSettingsPatternSerializer::ReadFromMessage(m, iter, pattern);
}

void ParamTraits<ContentSettingsPattern>::Log(
    const ContentSettingsPattern& p, std::string* l) {
  l->append("<ContentSettingsPattern: ");
  l->append(p.ToString());
  l->append(">");
}

}  // namespace IPC
