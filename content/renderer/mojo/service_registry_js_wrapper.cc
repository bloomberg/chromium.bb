// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/service_registry_js_wrapper.h"

#include <memory>
#include <utility>

#include "content/common/mojo/service_registry_impl.h"
#include "content/public/common/service_registry.h"
#include "mojo/edk/js/handle.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

gin::WrapperInfo ServiceRegistryJsWrapper::kWrapperInfo = {
    gin::kEmbedderNativeGin};
const char ServiceRegistryJsWrapper::kPerFrameModuleName[] =
    "content/public/renderer/frame_service_registry";
const char ServiceRegistryJsWrapper::kPerProcessModuleName[] =
    "content/public/renderer/service_registry";

ServiceRegistryJsWrapper::~ServiceRegistryJsWrapper() {
}

// static
gin::Handle<ServiceRegistryJsWrapper> ServiceRegistryJsWrapper::Create(
    v8::Isolate* isolate,
    v8::Handle<v8::Context> context,
    ServiceRegistry* service_registry) {
  return gin::CreateHandle(
      isolate,
      new ServiceRegistryJsWrapper(
          isolate, context,
          static_cast<ServiceRegistryImpl*>(service_registry)->GetWeakPtr()));
}

gin::ObjectTemplateBuilder ServiceRegistryJsWrapper::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<ServiceRegistryJsWrapper>::GetObjectTemplateBuilder(isolate)
      .SetMethod("connectToService",
                 &ServiceRegistryJsWrapper::ConnectToService)
      .SetMethod("addServiceOverrideForTesting",
                 &ServiceRegistryJsWrapper::AddServiceOverrideForTesting)
      .SetMethod("clearServiceOverridesForTesting",
                 &ServiceRegistryJsWrapper::ClearServiceOverridesForTesting);
}

mojo::Handle ServiceRegistryJsWrapper::ConnectToService(
    const std::string& service_name) {
  mojo::MessagePipe pipe;
  if (service_registry_)
    service_registry_->ConnectToRemoteService(service_name,
                                              std::move(pipe.handle0));
  return pipe.handle1.release();
}

void ServiceRegistryJsWrapper::AddServiceOverrideForTesting(
    const std::string& service_name,
    v8::Local<v8::Function> service_factory) {
  ServiceRegistry* registry = service_registry_.get();
  if (!registry)
    return;
  ScopedJsFactory factory(v8::Isolate::GetCurrent(), service_factory);
  registry->AddServiceOverrideForTesting(
      service_name, base::Bind(&ServiceRegistryJsWrapper::CallJsFactory,
                               weak_factory_.GetWeakPtr(), factory));
}

void ServiceRegistryJsWrapper::ClearServiceOverridesForTesting() {
  ServiceRegistryImpl* registry =
      static_cast<ServiceRegistryImpl*>(service_registry_.get());
  if (!registry)
    return;

  registry->ClearServiceOverridesForTesting();
}

ServiceRegistryJsWrapper::ServiceRegistryJsWrapper(
    v8::Isolate* isolate,
    v8::Handle<v8::Context> context,
    base::WeakPtr<ServiceRegistry> service_registry)
    : isolate_(isolate),
      context_(isolate, context),
      service_registry_(service_registry),
      weak_factory_(this) {
  context_.SetWeak(this, &ServiceRegistryJsWrapper::ClearContext,
                   v8::WeakCallbackType::kParameter);
}

void ServiceRegistryJsWrapper::CallJsFactory(
    const ScopedJsFactory& factory,
    mojo::ScopedMessagePipeHandle pipe) {
  if (context_.IsEmpty())
    return;

  v8::HandleScope handle_scope(isolate_);
  v8::Handle<v8::Context> context = context_.Get(isolate_);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> argv[] = {
      gin::ConvertToV8(isolate_, mojo::Handle(pipe.release().value()))};
  blink::WebLocalFrame::frameForContext(context)
      ->callFunctionEvenIfScriptDisabled(factory.Get(isolate_),
                                         v8::Undefined(isolate_), 1, argv);
}

// static
void ServiceRegistryJsWrapper::ClearContext(
    const v8::WeakCallbackInfo<ServiceRegistryJsWrapper>& data) {
  ServiceRegistryJsWrapper* service_registry = data.GetParameter();
  service_registry->context_.Reset();
}

}  // namespace content
