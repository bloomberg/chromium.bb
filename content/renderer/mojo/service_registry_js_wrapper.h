// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_SERVICE_REGISTRY_JS_WRAPPER_H_
#define CONTENT_RENDERER_MOJO_SERVICE_REGISTRY_JS_WRAPPER_H_

#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "v8/include/v8.h"

namespace content {

class ServiceRegistry;

// A JS wrapper around ServiceRegistry that allows connecting to remote
// services.
class CONTENT_EXPORT ServiceRegistryJsWrapper
    : public gin::Wrappable<ServiceRegistryJsWrapper> {
 public:
  ~ServiceRegistryJsWrapper() override;
  static gin::Handle<ServiceRegistryJsWrapper> Create(
      v8::Isolate* isolate,
      v8::Handle<v8::Context> context,
      ServiceRegistry* service_registry);

  // gin::Wrappable<ServiceRegistryJsWrapper> overrides.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // JS interface implementation.
  void AddServiceOverrideForTesting(const std::string& service_name,
                                    v8::Local<v8::Function> service_factory);
  void ClearServiceOverridesForTesting();
  mojo::Handle ConnectToService(const std::string& service_name);

  static gin::WrapperInfo kWrapperInfo;
  static const char kPerFrameModuleName[];
  static const char kPerProcessModuleName[];

 private:
  using ScopedJsFactory =
      v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>>;

  ServiceRegistryJsWrapper(v8::Isolate* isolate,
                           v8::Handle<v8::Context> context,
                           base::WeakPtr<ServiceRegistry> service_registry);

  void CallJsFactory(const ScopedJsFactory& factory,
                     mojo::ScopedMessagePipeHandle pipe);

  static void ClearContext(
      const v8::WeakCallbackInfo<ServiceRegistryJsWrapper>& data);

  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;
  base::WeakPtr<ServiceRegistry> service_registry_;

  base::WeakPtrFactory<ServiceRegistryJsWrapper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceRegistryJsWrapper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_SERVICE_REGISTRY_JS_WRAPPER_H_
