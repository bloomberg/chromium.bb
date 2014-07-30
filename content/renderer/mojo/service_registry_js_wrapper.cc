// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/service_registry_js_wrapper.h"

#include "content/common/mojo/service_registry_impl.h"
#include "content/public/common/service_registry.h"
#include "mojo/bindings/js/handle.h"

namespace content {

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
                &ServiceRegistryJsWrapper::ConnectToService);
}

mojo::Handle ServiceRegistryJsWrapper::ConnectToService(
    const std::string& service_name) {
  mojo::MessagePipe pipe;
  if (service_registry_)
    service_registry_->ConnectToRemoteService(service_name,
                                              pipe.handle0.Pass());
  return pipe.handle1.release();
}

ServiceRegistryJsWrapper::ServiceRegistryJsWrapper(
    base::WeakPtr<ServiceRegistry> service_registry)
    : service_registry_(service_registry) {
}

}  // namespace content
