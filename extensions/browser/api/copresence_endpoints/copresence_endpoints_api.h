// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_COPRESENCE_ENDPOINTS_COPRESENCE_ENDPOINTS_API_H_
#define EXTENSIONS_BROWSER_API_COPRESENCE_ENDPOINTS_COPRESENCE_ENDPOINTS_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace copresence_endpoints {
class CopresenceEndpoint;
}

namespace net {
class IOBuffer;
}

namespace extensions {

class CopresenceEndpointResource;

class CopresenceEndpointFunction : public UIThreadExtensionFunction {
 public:
  CopresenceEndpointFunction();

  void DispatchOnConnectedEvent(int endpoint_id);

  // Needs to be used from CreateLocalEndpoint.
  void OnDataReceived(int endpoint_id,
                      const scoped_refptr<net::IOBuffer>& buffer,
                      int size);

 protected:
  ~CopresenceEndpointFunction() override;

  // Override this and do actual work here.
  virtual ExtensionFunction::ResponseAction Execute() = 0;

  // Takes ownership of endpoint.
  int AddEndpoint(CopresenceEndpointResource* endpoint);

  // Takes ownership of endpoint.
  void ReplaceEndpoint(const std::string& extension_id,
                       int endpoint_id,
                       CopresenceEndpointResource* endpoint);

  CopresenceEndpointResource* GetEndpoint(int endpoint_id);

  void RemoveEndpoint(int endpoint_id);

  // ExtensionFunction overrides:
  ExtensionFunction::ResponseAction Run() override;

 private:
  void Initialize();

  void DispatchOnReceiveEvent(int local_endpoint_id,
                              int remote_endpoint_id,
                              const std::string& data);

  ApiResourceManager<CopresenceEndpointResource>* endpoints_manager_;
};

class CopresenceEndpointsCreateLocalEndpointFunction
    : public CopresenceEndpointFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresenceEndpoints.createLocalEndpoint",
                             COPRESENCEENDPOINTS_CREATELOCALENDPOINT);

 protected:
  ~CopresenceEndpointsCreateLocalEndpointFunction() override {}
  ExtensionFunction::ResponseAction Execute() override;

 private:
  void OnCreated(int endpoint_id, const std::string& locator);
};

class CopresenceEndpointsDestroyEndpointFunction
    : public CopresenceEndpointFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresenceEndpoints.destroyLocalEndpoint",
                             COPRESENCEENDPOINTS_DESTROYLOCALENDPOINT);

 protected:
  ~CopresenceEndpointsDestroyEndpointFunction() override {}
  ExtensionFunction::ResponseAction Execute() override;
};

class CopresenceEndpointsSendFunction : public CopresenceEndpointFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresenceEndpoints.send",
                             COPRESENCEENDPOINTS_SEND);

 protected:
  ~CopresenceEndpointsSendFunction() override {}
  ExtensionFunction::ResponseAction Execute() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_COPRESENCE_ENDPOINTS_COPRESENCE_ENDPOINTS_API_H_
