// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONTENT_SETTINGS_PATTERN_SERIALIZER_H_
#define CHROME_COMMON_CONTENT_SETTINGS_PATTERN_SERIALIZER_H_

#include "base/macros.h"

namespace IPC {
class Message;
}

class ContentSettingsPattern;
class PickleIterator;

class ContentSettingsPatternSerializer {
 public:
  // Serializes the pattern to an IPC message.
  static void WriteToMessage(const ContentSettingsPattern& pattern,
                             IPC::Message* m);
  // Deserializes the pattern from the IPC message.
  static bool ReadFromMessage(const IPC::Message* m, PickleIterator* iter,
                              ContentSettingsPattern* pattern);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingsPatternSerializer);
};

#endif  // CHROME_COMMON_CONTENT_SETTINGS_PATTERN_SERIALIZER_H_
