// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "error_cache_load.h"

#include "content/public/renderer/render_frame.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"

gin::WrapperInfo ErrorCacheLoad::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void ErrorCacheLoad::Install(content::RenderFrame* render_frame,
                             const GURL& page_url) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context =
      render_frame->GetWebFrame()->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<ErrorCacheLoad> controller =
      gin::CreateHandle(isolate, new ErrorCacheLoad(render_frame, page_url));
  v8::Handle<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "errorCacheLoad"), controller.ToV8());
}

bool ErrorCacheLoad::ReloadStaleInstance() {
  if (!render_frame_)
    return false;

  blink::WebURLRequest request(page_url_);
  request.setCachePolicy(blink::WebURLRequest::ReturnCacheDataDontLoad);

  render_frame_->GetWebFrame()->loadRequest(request);

  return true;
}

ErrorCacheLoad::ErrorCacheLoad(content::RenderFrame* render_frame,
                               const GURL& page_url)
    : RenderFrameObserver(render_frame),
      render_frame_(render_frame),
      page_url_(page_url) {}

ErrorCacheLoad::~ErrorCacheLoad() {}

gin::ObjectTemplateBuilder ErrorCacheLoad::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<ErrorCacheLoad>::GetObjectTemplateBuilder(isolate)
      .SetMethod("reloadStaleInstance", &ErrorCacheLoad::ReloadStaleInstance);
}

void ErrorCacheLoad::OnDestruct() { render_frame_ = NULL; }
