// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages.h"

namespace IPC {

void ParamTraits<ContentSettings>::Write(
    Message* m, const ContentSettings& settings) {
  for (size_t i = 0; i < arraysize(settings.settings); ++i)
    WriteParam(m, settings.settings[i]);
}

bool ParamTraits<ContentSettings>::Read(
    const Message* m, void** iter, ContentSettings* r) {
  for (size_t i = 0; i < arraysize(r->settings); ++i) {
    if (!ReadParam(m, iter, &r->settings[i]))
      return false;
  }
  return true;
}

void ParamTraits<ContentSettings>::Log(
    const ContentSettings& p, std::string* l) {
  l->append("<ContentSettings>");
}

void ParamTraits<ContentSettingsPattern>::Write(
    Message* m, const ContentSettingsPattern& pattern) {
  pattern.WriteToMessage(m);
}

bool ParamTraits<ContentSettingsPattern>::Read(
    const Message* m, void** iter, ContentSettingsPattern* pattern) {
  return pattern->ReadFromMessage(m, iter);
}

void ParamTraits<ContentSettingsPattern>::Log(
    const ContentSettingsPattern& p, std::string* l) {
  l->append("<ContentSettingsPattern: ");
  l->append(p.ToString());
  l->append(">");
}

}  // namespace IPC
