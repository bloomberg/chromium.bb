// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_BINDING_HOOKS_TEST_DELEGATE_H_
#define EXTENSIONS_RENDERER_API_BINDING_HOOKS_TEST_DELEGATE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "extensions/renderer/api_binding_hooks_delegate.h"
#include "extensions/renderer/api_binding_types.h"
#include "v8/include/v8.h"

namespace extensions {

// A test class to simply adding custom events or handlers for API hooks.
class APIBindingHooksTestDelegate : public APIBindingHooksDelegate {
 public:
  APIBindingHooksTestDelegate();
  ~APIBindingHooksTestDelegate() override;

  using CustomEventFactory = base::Callback<v8::Local<v8::Value>(
      v8::Local<v8::Context>,
      const binding::RunJSFunctionSync& run_js,
      const std::string& event_name)>;

  using RequestHandler = base::Callback<APIBindingHooks::RequestResult(
      const APISignature*,
      v8::Local<v8::Context> context,
      std::vector<v8::Local<v8::Value>>*,
      const APITypeReferenceMap&)>;

  // Adds a custom |handler| for the method with the given |name|.
  void AddHandler(base::StringPiece name, const RequestHandler& handler);

  // Creates events with the given factory.
  void SetCustomEvent(const CustomEventFactory& custom_event);

  // APIBindingHooksDelegate:
  bool CreateCustomEvent(v8::Local<v8::Context> context,
                         const binding::RunJSFunctionSync& run_js_sync,
                         const std::string& event_name,
                         v8::Local<v8::Value>* event_out) override;
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      std::vector<v8::Local<v8::Value>>* arguments,
      const APITypeReferenceMap& refs) override;

 private:
  std::map<std::string, RequestHandler> request_handlers_;
  CustomEventFactory custom_event_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingHooksTestDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_BINDING_HOOKS_TEST_DELEGATE_H_
