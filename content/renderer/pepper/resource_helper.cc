// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/resource_helper.h"

#include "base/logging.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/ppapi_plugin_instance_impl.h"
#include "ppapi/shared_impl/resource.h"

namespace webkit {
namespace ppapi {

// static
PluginInstanceImpl* ResourceHelper::GetPluginInstance(
    const ::ppapi::Resource* resource) {
  return PPInstanceToPluginInstance(resource->pp_instance());
}

PluginInstanceImpl* ResourceHelper::PPInstanceToPluginInstance(
    PP_Instance instance) {
  return HostGlobals::Get()->GetInstance(instance);
}

PluginModule* ResourceHelper::GetPluginModule(
    const ::ppapi::Resource* resource) {
  PluginInstanceImpl* instance = GetPluginInstance(resource);
  return instance ? instance->module() : NULL;
}

PluginDelegate* ResourceHelper::GetPluginDelegate(
    const ::ppapi::Resource* resource) {
  PluginInstanceImpl* instance = GetPluginInstance(resource);
  return instance ? instance->delegate() : NULL;
}

}  // namespace ppapi
}  // namespace webkit

