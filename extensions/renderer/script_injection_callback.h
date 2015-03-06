// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTION_CALLBACK_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTION_CALLBACK_H_

#include "base/macros.h"
#include "third_party/WebKit/public/web/WebScriptExecutionCallback.h"
#include "v8/include/v8.h"

namespace blink {
class WebLocalFrame;
template<typename T> class WebVector;
}

namespace extensions {
class ScriptInjection;

// A wrapper around a callback to notify a script injection when injection
// completes.
// This class manages its own lifetime.
class ScriptInjectionCallback : public blink::WebScriptExecutionCallback {
 public:
  ScriptInjectionCallback(ScriptInjection* injection,
                          blink::WebLocalFrame* web_frame);
  ~ScriptInjectionCallback() override;

  void completed(
      const blink::WebVector<v8::Local<v8::Value> >& result) override;

 private:
  ScriptInjection* injection_;

  blink::WebLocalFrame* web_frame_;

  DISALLOW_COPY_AND_ASSIGN(ScriptInjectionCallback);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_INJECTION_CALLBACK_H_
