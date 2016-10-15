// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_INTERFACE_REGISTRY_JS_WRAPPER_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_INTERFACE_REGISTRY_JS_WRAPPER_H_

#include <string>

#include "base/macros.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "v8/include/v8.h"

namespace service_manager {
class InterfaceRegistry;
}

namespace content {

// A JS wrapper around service_manager::InterfaceRegistry that allows connecting
// to
// interfaces exposed by the renderer for testing.
class InterfaceRegistryJsWrapper
    : public gin::Wrappable<InterfaceRegistryJsWrapper> {
 public:
  static gin::Handle<InterfaceRegistryJsWrapper> Create(
      v8::Isolate* isolate,
      v8::Handle<v8::Context> context,
      service_manager::InterfaceRegistry* interface_registry);

  // gin::Wrappable<InterfaceRegistryJsWrapper> overrides.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // JS interface implementation.
  mojo::Handle GetLocalInterfaceForTesting(const std::string& interface_name);

  static gin::WrapperInfo kWrapperInfo;
  static const char kPerFrameModuleName[];
  static const char kPerProcessModuleName[];

 private:
  using ScopedJsFactory =
      v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>>;

  InterfaceRegistryJsWrapper(
      v8::Isolate* isolate,
      v8::Handle<v8::Context> context,
      base::WeakPtr<service_manager::InterfaceRegistry> interface_registry);
  ~InterfaceRegistryJsWrapper() override;

  void CallJsFactory(const ScopedJsFactory& factory,
                     mojo::ScopedMessagePipeHandle pipe);

  static void ClearContext(
      const v8::WeakCallbackInfo<InterfaceRegistryJsWrapper>& data);

  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;
  base::WeakPtr<service_manager::InterfaceRegistry> interface_registry_;

  base::WeakPtrFactory<InterfaceRegistryJsWrapper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceRegistryJsWrapper);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_INTERFACE_REGISTRY_JS_WRAPPER_H_
