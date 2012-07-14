// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_instance_state_accessor_impl.h"

#include "ppapi/shared_impl/ppapi_permissions.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using webkit::ppapi::HostGlobals;
using webkit::ppapi::PluginInstance;

namespace content {

PepperInstanceStateAccessorImpl::PepperInstanceStateAccessorImpl(
    webkit::ppapi::PluginModule* module)
    : module_(module) {
}

PepperInstanceStateAccessorImpl::~PepperInstanceStateAccessorImpl() {
}

bool PepperInstanceStateAccessorImpl::IsValidInstance(PP_Instance instance) {
  return !!GetAndValidateInstance(instance);
}

bool PepperInstanceStateAccessorImpl::HasUserGesture(PP_Instance pp_instance) {
  PluginInstance* instance = GetAndValidateInstance(pp_instance);
  if (!instance)
    return false;

  if (instance->module()->permissions().HasPermission(
          ppapi::PERMISSION_BYPASS_USER_GESTURE))
    return true;
  return instance->IsProcessingUserGesture();
}

PluginInstance* PepperInstanceStateAccessorImpl::GetAndValidateInstance(
    PP_Instance pp_instance) {
  PluginInstance* instance = HostGlobals::Get()->GetInstance(pp_instance);
  if (!instance)
    return NULL;
  if (instance->module() != module_)
    return NULL;
  return instance;
}

}  // namespace content
