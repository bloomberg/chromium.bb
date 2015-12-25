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
#include "mojo/public/cpp/system/core.h"

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
      ServiceRegistry* service_registry);

  // gin::Wrappable<ServiceRegistryJsWrapper> overrides.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // JS interface implementation.
  mojo::Handle ConnectToService(const std::string& service_name);

  static gin::WrapperInfo kWrapperInfo;
  static const char kModuleName[];

 private:
  explicit ServiceRegistryJsWrapper(
      base::WeakPtr<ServiceRegistry> service_registry);

  base::WeakPtr<ServiceRegistry> service_registry_;

  DISALLOW_COPY_AND_ASSIGN(ServiceRegistryJsWrapper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_SERVICE_REGISTRY_JS_WRAPPER_H_
