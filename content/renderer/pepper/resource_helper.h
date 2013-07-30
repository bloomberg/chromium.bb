// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_RESOURCE_HELPER_H_
#define CONTENT_RENDERER_PEPPER_RESOURCE_HELPER_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"

namespace ppapi {
class Resource;
}

namespace content {

class PepperHelperImpl;
class PepperPluginInstanceImpl;
class PluginModule;

// Helper functions for Resoruce implementations.
//
// This is specifically not designed to be a base class that derives from
// ppapi::Resource to avoid diamond inheritance if most of a resource class
// is implemented in the shared_impl (to share code with the proxy).
class ResourceHelper {
 public:
  // Returns the instance implementation object for the given resource, or NULL
  // if the resource has outlived its instance.
  static PepperPluginInstanceImpl* GetPluginInstance(
      const ::ppapi::Resource* resource);

  // Returns the module for the given resource, or NULL if the resource has
  // outlived its instance.
  static PluginModule* GetPluginModule(const ::ppapi::Resource* resource);

  // Returns the plugin delegate for the given resource, or NULL if the
  // resource has outlived its instance.
  static PepperHelperImpl* GetHelper(const ::ppapi::Resource* resource);

  // Returns the instance implementation object for the pp_instance.
  static PepperPluginInstanceImpl* PPInstanceToPluginInstance(
      PP_Instance instance);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ResourceHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_RESOURCE_IMPL_HELPER_H_
