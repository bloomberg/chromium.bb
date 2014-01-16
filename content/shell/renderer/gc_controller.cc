// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/gc_controller.h"

#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "v8/include/v8.h"

namespace content {

gin::WrapperInfo GCController::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void GCController::Install(blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<GCController> controller =
      gin::CreateHandle(isolate, new GCController());
  v8::Handle<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "GCController"), controller.ToV8());
}

GCController::GCController() {}

GCController::~GCController() {}

gin::ObjectTemplateBuilder GCController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<GCController>::GetObjectTemplateBuilder(isolate)
      .SetMethod("collect", &GCController::Collect)
      .SetMethod("minorCollect", &GCController::MinorCollect);
}

void GCController::Collect(const gin::Arguments& args) {
  args.isolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

void GCController::MinorCollect(const gin::Arguments& args) {
  args.isolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kMinorGarbageCollection);
}

}  // namespace content
