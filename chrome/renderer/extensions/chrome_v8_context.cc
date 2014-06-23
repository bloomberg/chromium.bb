// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_context.h"

namespace extensions {

ChromeV8Context::ChromeV8Context(const v8::Handle<v8::Context>& v8_context,
                                 blink::WebFrame* web_frame,
                                 const Extension* extension,
                                 Feature::Context context_type)
    : ScriptContext(v8_context, web_frame, extension, context_type) {
}

}  // namespace extensions
