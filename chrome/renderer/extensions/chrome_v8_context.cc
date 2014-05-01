// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_context.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/user_script_slave.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "v8/include/v8.h"

namespace extensions {

ChromeV8Context::ChromeV8Context(const v8::Handle<v8::Context>& v8_context,
                                 blink::WebFrame* web_frame,
                                 const Extension* extension,
                                 Feature::Context context_type)
    : ScriptContext(v8_context, web_frame, extension, context_type),
      pepper_request_proxy_(this) {
}

}  // namespace extensions
