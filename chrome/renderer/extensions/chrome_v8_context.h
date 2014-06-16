// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/request_sender.h"
#include "extensions/renderer/safe_builtins.h"
#include "extensions/renderer/scoped_persistent.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8.h"

namespace blink {
class WebFrame;
}

namespace content {
class RenderView;
}

namespace extensions {
class Extension;

// Chrome's wrapper for a v8 context.
class ChromeV8Context : public ScriptContext {
 public:
  ChromeV8Context(const v8::Handle<v8::Context>& context,
                  blink::WebFrame* frame,
                  const Extension* extension,
                  Feature::Context context_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeV8Context);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_V8_CONTEXT_H_
