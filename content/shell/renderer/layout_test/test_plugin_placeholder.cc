// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/test_plugin_placeholder.h"

#include "gin/handle.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace content {

TestPluginPlaceholder::TestPluginPlaceholder(
    RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params)
    : PluginPlaceholder(render_frame,
                        frame,
                        params,
                        "<div>Test content</div>",
                        GURL("http://www.test.com")) {
}

void TestPluginPlaceholder::BindWebFrame(blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  DCHECK(!context.IsEmpty());

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "plugin"),
              gin::CreateHandle(isolate, this).ToV8());
}

}  // namespace content
