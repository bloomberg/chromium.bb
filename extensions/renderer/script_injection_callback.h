// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTION_CALLBACK_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTION_CALLBACK_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "v8/include/v8.h"

namespace extensions {

// A wrapper around a callback to notify a script injection when injection
// completes.
// This class manages its own lifetime.
class ScriptInjectionCallback : public blink::WebScriptExecutionCallback {
 public:
  using CompleteCallback =
      base::Callback<void(const std::vector<v8::Local<v8::Value>>& result)>;

  ScriptInjectionCallback(const CompleteCallback& injection_completed_callback);
  ~ScriptInjectionCallback() override;

  void Completed(const blink::WebVector<v8::Local<v8::Value>>& result) override;

 private:
  CompleteCallback injection_completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScriptInjectionCallback);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_INJECTION_CALLBACK_H_
