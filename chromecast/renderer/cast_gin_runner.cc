// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_gin_runner.h"

#include "content/public/renderer/render_frame.h"
#include "gin/per_context_data.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"

namespace chromecast {
namespace shell {

namespace {
const void* kCastContextData;
const void* kCastGinRunnerKey = static_cast<const void*>(&kCastContextData);
}

// static
CastGinRunner* CastGinRunner::Get(content::RenderFrame* render_frame) {
  DCHECK(render_frame);
  blink::WebFrame* frame = render_frame->GetWebFrame();
  v8::HandleScope handle_scope(blink::mainThreadIsolate());
  gin::PerContextData* context_data =
      gin::PerContextData::From(frame->mainWorldScriptContext());
  CastGinRunner* runner =
      static_cast<CastGinRunner*>(context_data->GetUserData(kCastGinRunnerKey));
  return runner ? runner : new CastGinRunner(frame, context_data);
}

CastGinRunner::CastGinRunner(blink::WebFrame* frame,
                             gin::PerContextData* context_data)
    : frame_(frame), context_holder_(context_data->context_holder()) {
  DCHECK(frame_);
  DCHECK(context_holder_);

  // context_data takes ownership of this class.
  context_data->SetUserData(kCastGinRunnerKey, this);

  // Note: this installs the runner globally. If we need to support more than
  // one runner at a time we'll have to revisit this.
  context_data->set_runner(this);
}

CastGinRunner::~CastGinRunner() {}

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

}  // namespace shell
}  // namespace chromecast
