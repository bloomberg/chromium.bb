// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/tts_custom_bindings.h"

#include <string>

#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace extensions {

TTSCustomBindings::TTSCustomBindings()
    : ChromeV8Extension(NULL) {
  RouteStaticFunction("GetNextTTSEventId", &GetNextTTSEventId);
}

// static
v8::Handle<v8::Value> TTSCustomBindings::GetNextTTSEventId(
    const v8::Arguments& args) {
  // Note: this works because the TTS API only works in the
  // extension process, not content scripts.
  static int next_tts_event_id = 1;
  return v8::Integer::New(next_tts_event_id++);
}

}  // extensions
