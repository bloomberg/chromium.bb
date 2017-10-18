// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_BLINK_CONNECTOR_JS_WRAPPER_H_
#define CONTENT_RENDERER_MOJO_BLINK_CONNECTOR_JS_WRAPPER_H_

#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "v8/include/v8.h"

namespace service_manager {
class Connector;
}

namespace content {

// A JS wrapper around service_manager::Connector that allows connecting to
// remote services.
class CONTENT_EXPORT BlinkConnectorJsWrapper
    : public gin::Wrappable<BlinkConnectorJsWrapper> {
 public:
  ~BlinkConnectorJsWrapper() override;
  static gin::Handle<BlinkConnectorJsWrapper> Create(
      v8::Isolate* isolate,
      v8::Local<v8::Context> context,
      service_manager::Connector* remote_interfaces);

  // gin::Wrappable<BlinkConnectorJsWrapper> overrides.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // JS interface implementation.
  mojo::Handle BindInterface(const std::string& service_name,
                             const std::string& interface_name);

  static gin::WrapperInfo kWrapperInfo;
  static const char kModuleName[];

 private:
  using ScopedJsFactory =
      v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>>;

  BlinkConnectorJsWrapper(v8::Isolate* isolate,
                          v8::Local<v8::Context> context,
                          base::WeakPtr<service_manager::Connector> connector);

  void CallJsFactory(const ScopedJsFactory& factory,
                     mojo::ScopedMessagePipeHandle pipe);

  static void ClearContext(
      const v8::WeakCallbackInfo<BlinkConnectorJsWrapper>& data);

  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;
  base::WeakPtr<service_manager::Connector> connector_;

  base::WeakPtrFactory<BlinkConnectorJsWrapper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlinkConnectorJsWrapper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_BLINK_CONNECTOR_JS_WRAPPER_H_
