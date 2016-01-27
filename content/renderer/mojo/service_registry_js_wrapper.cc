// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/service_registry_js_wrapper.h"

#include <utility>

#include "base/memory/scoped_ptr.h"
#include "content/common/mojo/service_registry_impl.h"
#include "content/public/common/service_registry.h"
#include "third_party/mojo/src/mojo/edk/js/handle.h"
#include "v8/include/v8.h"

namespace content {

namespace {

void CallJsFactory(scoped_ptr<v8::Persistent<v8::Function>> factory,
                   mojo::ScopedMessagePipeHandle pipe) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Value> argv[] = {
    gin::ConvertToV8(isolate, mojo::Handle(pipe.release().value()))
  };
  factory->Get(isolate)->Call(v8::Undefined(isolate), 1, argv);
}

}  // namespace

gin::WrapperInfo ServiceRegistryJsWrapper::kWrapperInfo = {
    gin::kEmbedderNativeGin};
const char ServiceRegistryJsWrapper::kModuleName[] =
    "content/public/renderer/service_provider";

ServiceRegistryJsWrapper::~ServiceRegistryJsWrapper() {
}

// static
gin::Handle<ServiceRegistryJsWrapper> ServiceRegistryJsWrapper::Create(
    v8::Isolate* isolate,
    ServiceRegistry* service_registry) {
  return gin::CreateHandle(
      isolate,
      new ServiceRegistryJsWrapper(
          static_cast<ServiceRegistryImpl*>(service_registry)->GetWeakPtr()));
}

gin::ObjectTemplateBuilder ServiceRegistryJsWrapper::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<ServiceRegistryJsWrapper>::GetObjectTemplateBuilder(isolate).
      SetMethod("connectToService",
                &ServiceRegistryJsWrapper::ConnectToService).
      SetMethod("addServiceOverrideForTesting",
                &ServiceRegistryJsWrapper::AddServiceOverrideForTesting);
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
  ServiceRegistryImpl* registry =
      static_cast<ServiceRegistryImpl*>(service_registry_.get());
  if (!registry)
    return;
  scoped_ptr<v8::Persistent<v8::Function>> factory(
      new v8::Persistent<v8::Function>(v8::Isolate::GetCurrent(),
                                       service_factory));
  registry->AddServiceOverrideForTesting(
      service_name, base::Bind(&CallJsFactory, base::Passed(&factory)));
}

ServiceRegistryJsWrapper::ServiceRegistryJsWrapper(
    base::WeakPtr<ServiceRegistry> service_registry)
    : service_registry_(service_registry) {
}

}  // namespace content
