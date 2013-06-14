// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/tts_custom_bindings.h"

#include <string>

#include "base/bind.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace extensions {

TTSCustomBindings::TTSCustomBindings(
    Dispatcher* dispatcher, ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction("GetNextTTSEventId",
      base::Bind(&TTSCustomBindings::GetNextTTSEventId,
                 base::Unretained(this)));
}

void TTSCustomBindings::GetNextTTSEventId(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Note: this works because the TTS API only works in the
  // extension process, not content scripts.
  static int32_t next_tts_event_id = 1;
  args.GetReturnValue().Set(next_tts_event_id++);
}

}  // namespace extensions
