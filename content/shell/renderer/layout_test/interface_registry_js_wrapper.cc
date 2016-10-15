// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/interface_registry_js_wrapper.h"

#include <memory>
#include <utility>

#include "mojo/edk/js/handle.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace content {

gin::WrapperInfo InterfaceRegistryJsWrapper::kWrapperInfo = {
    gin::kEmbedderNativeGin};
const char InterfaceRegistryJsWrapper::kPerFrameModuleName[] =
    "content/shell/renderer/layout_test/frame_interface_registry";
const char InterfaceRegistryJsWrapper::kPerProcessModuleName[] =
    "content/shell/renderer/layout_test/interface_registry";

InterfaceRegistryJsWrapper::~InterfaceRegistryJsWrapper() = default;

// static
gin::Handle<InterfaceRegistryJsWrapper> InterfaceRegistryJsWrapper::Create(
    v8::Isolate* isolate,
    v8::Handle<v8::Context> context,
    service_manager::InterfaceRegistry* interface_registry) {
  return gin::CreateHandle(
      isolate, new InterfaceRegistryJsWrapper(
                   isolate, context, interface_registry->GetWeakPtr()));
}

gin::ObjectTemplateBuilder InterfaceRegistryJsWrapper::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<InterfaceRegistryJsWrapper>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("getLocalInterfaceForTesting",
                 &InterfaceRegistryJsWrapper::GetLocalInterfaceForTesting);
}

mojo::Handle InterfaceRegistryJsWrapper::GetLocalInterfaceForTesting(
    const std::string& interface_name) {
  mojo::MessagePipe pipe;
  if (interface_registry_) {
    service_manager::InterfaceRegistry::TestApi test_api(
        interface_registry_.get());
    test_api.GetLocalInterface(interface_name, std::move(pipe.handle0));
  }
  return pipe.handle1.release();
}

InterfaceRegistryJsWrapper::InterfaceRegistryJsWrapper(
    v8::Isolate* isolate,
    v8::Handle<v8::Context> context,
    base::WeakPtr<service_manager::InterfaceRegistry> interface_registry)
    : isolate_(isolate),
      context_(isolate, context),
      interface_registry_(interface_registry),
      weak_factory_(this) {
  context_.SetWeak(this, &InterfaceRegistryJsWrapper::ClearContext,
                   v8::WeakCallbackType::kParameter);
}

// static
void InterfaceRegistryJsWrapper::ClearContext(
    const v8::WeakCallbackInfo<InterfaceRegistryJsWrapper>& data) {
  InterfaceRegistryJsWrapper* registry = data.GetParameter();
  registry->context_.Reset();
}

}  // namespace content
