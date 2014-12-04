// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_COPRESENCE_ENDPOINTS_COPRESENCE_ENDPOINTS_RESOURCES_H_
#define EXTENSIONS_BROWSER_API_COPRESENCE_ENDPOINTS_COPRESENCE_ENDPOINTS_RESOURCES_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"

namespace copresence_endpoints {
class CopresenceEndpoint;
}

namespace extensions {

class CopresenceEndpointResource : public ApiResource {
 public:
  CopresenceEndpointResource(
      const std::string& owner_extension_id,
      scoped_ptr<copresence_endpoints::CopresenceEndpoint> endpoint);
  ~CopresenceEndpointResource() override;

  copresence_endpoints::CopresenceEndpoint* endpoint();

  std::string& packet() { return packet_; }

  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

 private:
  friend class ApiResourceManager<CopresenceEndpointResource>;
  static const char* service_name() {
    return "CopresenceEndpointResourceManager";
  }

  scoped_ptr<copresence_endpoints::CopresenceEndpoint> endpoint_;
  std::string packet_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceEndpointResource);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_COPRESENCE_ENDPOINTS_COPRESENCE_ENDPOINTS_RESOURCES_H_
