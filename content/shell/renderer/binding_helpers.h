// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BINDING_HELPERS_H_
#define CONTENT_SHELL_BINDING_HELPERS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace content {

template<class WrappedClass>
void InstallAsWindowProperties(WrappedClass* wrapped,
                               blink::WebFrame* frame,
                               const std::vector<std::string>& names) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<WrappedClass> bindings = gin::CreateHandle(isolate, wrapped);
  if (bindings.IsEmpty())
    return;
  v8::Handle<v8::Object> global = context->Global();
  v8::Handle<v8::Value> v8_bindings = bindings.ToV8();
  for (size_t i = 0; i < names.size(); ++i)
    global->Set(gin::StringToV8(isolate, names[i].c_str()), v8_bindings);
}

}  // namespace content

#endif  // CONTENT_SHELL_BINDING_HELPERS_DISPATCHER_H_
