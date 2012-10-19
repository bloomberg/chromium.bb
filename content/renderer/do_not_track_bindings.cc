// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/do_not_track_bindings.h"

#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;

namespace content {

namespace {

v8::Handle<v8::Value> GetDoNotTrack(v8::Local<v8::String> property,
                                    const v8::AccessorInfo& info) {
  WebFrame* frame = WebFrame::frameForCurrentContext();
  if (!frame)
    return v8::Null();
  RenderViewImpl* view = RenderViewImpl::FromWebView(frame->view());
  if (!view)
    return v8::Null();
  if (view->enable_do_not_track())
    return v8::String::New("1");
  return v8::Null();
}

}  // namespace

void InjectDoNotTrackBindings(WebFrame* frame) {
  v8::HandleScope handle_scope;

  v8::Handle<v8::Context> v8_context = frame->mainWorldScriptContext();
  if (v8_context.IsEmpty())
    return;

  v8::Context::Scope scope(v8_context);

  v8::Handle<v8::Object> global = v8_context->Global();
  v8::Handle<v8::Object> navigator =
      global->Get(v8::String::New("navigator"))->ToObject();
  if (navigator.IsEmpty())
    return;
  navigator->SetAccessor(v8::String::New("doNotTrack"), GetDoNotTrack);
}

}  // namespace content
