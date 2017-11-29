// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_js_runner.h"

#include "content/public/renderer/worker_thread.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

ExtensionJSRunner::ExtensionJSRunner(ScriptContext* script_context)
    : script_context_(script_context) {}
ExtensionJSRunner::~ExtensionJSRunner() {}

void ExtensionJSRunner::RunJSFunction(v8::Local<v8::Function> function,
                                      v8::Local<v8::Context> context,
                                      int argc,
                                      v8::Local<v8::Value> argv[]) {
  DCHECK(script_context_->v8_context() == context);

  // TODO(devlin): Move ScriptContext::SafeCallFunction() into here?
  script_context_->SafeCallFunction(function, argc, argv);
}

v8::Global<v8::Value> ExtensionJSRunner::RunJSFunctionSync(
    v8::Local<v8::Function> function,
    v8::Local<v8::Context> context,
    int argc,
    v8::Local<v8::Value> argv[]) {
  DCHECK(script_context_->v8_context() == context);

  bool did_complete = false;
  v8::Global<v8::Value> result;
  auto callback = base::Bind(
      [](v8::Isolate* isolate, bool* did_complete_out,
         v8::Global<v8::Value>* result_out,
         const std::vector<v8::Local<v8::Value>>& results) {
        *did_complete_out = true;
        // The locals are released after the callback is executed, so we need to
        // grab a persistent handle.
        if (!results.empty() && !results[0].IsEmpty())
          result_out->Reset(isolate, results[0]);
      },
      base::Unretained(context->GetIsolate()), base::Unretained(&did_complete),
      base::Unretained(&result));

  script_context_->SafeCallFunction(function, argc, argv, callback);
  CHECK(did_complete) << "expected script to execute synchronously";
  return result;
}

}  // namespace extensions
