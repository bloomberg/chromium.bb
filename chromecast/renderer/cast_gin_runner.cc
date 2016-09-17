// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_gin_runner.h"

#include "content/public/renderer/render_frame.h"
#include "gin/per_context_data.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"

namespace chromecast {
namespace shell {

CastGinRunner::CastGinRunner(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      frame_(render_frame->GetWebFrame()),
      context_holder_(
          gin::PerContextData::From(frame_->mainWorldScriptContext())
              ->context_holder()) {
  DCHECK(frame_);
  DCHECK(context_holder_);
  gin::PerContextData* context_data =
      gin::PerContextData::From(frame_->mainWorldScriptContext());

  v8::Isolate::Scope isolate_scope(context_holder_->isolate());
  v8::HandleScope handle_scope(context_holder_->isolate());

  context_data->SetUserData(kCastContextData, this);

  // Note: this installs the runner globally. If we need to support more than
  // one runner at a time we'll have to revisit this.
  context_data->set_runner(this);
}

CastGinRunner::~CastGinRunner() {
  RemoveUserData(render_frame()->GetWebFrame()->mainWorldScriptContext());
}

void CastGinRunner::Run(const std::string& source,
                        const std::string& resource_name) {
  frame_->executeScript(
      blink::WebScriptSource(blink::WebString::fromUTF8(source)));
}

v8::Local<v8::Value> CastGinRunner::Call(v8::Local<v8::Function> function,
                                         v8::Local<v8::Value> receiver,
                                         int argc,
                                         v8::Local<v8::Value> argv[]) {
  return frame_->callFunctionEvenIfScriptDisabled(function, receiver, argc,
                                                  argv);
}

gin::ContextHolder* CastGinRunner::GetContextHolder() {
  return context_holder_;
}

void CastGinRunner::WillReleaseScriptContext(v8::Local<v8::Context> context,
                                             int world_id) {
  RemoveUserData(context);
}

void CastGinRunner::DidClearWindowObject() {
  RemoveUserData(render_frame()->GetWebFrame()->mainWorldScriptContext());
}

void CastGinRunner::OnDestruct() {
  RemoveUserData(render_frame()->GetWebFrame()->mainWorldScriptContext());
}

void CastGinRunner::RemoveUserData(v8::Local<v8::Context> context) {
  gin::PerContextData* context_data = gin::PerContextData::From(context);
  if (!context_data)
    return;

  context_data->RemoveUserData(kCastContextData);
}

}  // namespace shell
}  // namespace chromecast
