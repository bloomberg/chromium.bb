// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/copresence_endpoints/copresence_endpoint_resource.h"

#include "base/lazy_instance.h"
#include "components/copresence_endpoints/public/copresence_endpoint.h"
#include "content/public/browser/browser_context.h"

using copresence_endpoints::CopresenceEndpoint;

namespace extensions {

// CopresenceEndpointResource.

// static
static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<CopresenceEndpointResource>>> g_endpoint_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<CopresenceEndpointResource>>*
ApiResourceManager<CopresenceEndpointResource>::GetFactoryInstance() {
  return g_endpoint_factory.Pointer();
}

CopresenceEndpointResource::CopresenceEndpointResource(
    const std::string& owner_extension_id,
    scoped_ptr<copresence_endpoints::CopresenceEndpoint> endpoint)
    : ApiResource(owner_extension_id), endpoint_(endpoint.Pass()) {
}

CopresenceEndpointResource::~CopresenceEndpointResource() {
}

copresence_endpoints::CopresenceEndpoint*
CopresenceEndpointResource::endpoint() {
  return endpoint_.get();
}

}  // namespace extensions
