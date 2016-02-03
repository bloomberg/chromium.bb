// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/content_settings_pattern_serializer.h"

#include "chrome/common/render_messages.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

// static
void ContentSettingsPatternSerializer::WriteToMessage(
    const ContentSettingsPattern& pattern,
    base::Pickle* m) {
  IPC::WriteParam(m, pattern.is_valid_);
  IPC::WriteParam(m, pattern.parts_);
}

// static
bool ContentSettingsPatternSerializer::ReadFromMessage(
    const base::Pickle* m,
    base::PickleIterator* iter,
    ContentSettingsPattern* pattern) {
  DCHECK(pattern);
  return IPC::ReadParam(m, iter, &pattern->is_valid_) &&
         IPC::ReadParam(m, iter, &pattern->parts_);
}
