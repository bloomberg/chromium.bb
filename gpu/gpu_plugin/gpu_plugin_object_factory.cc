// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gpu_plugin/gpu_plugin_object.h"
#include "gpu/gpu_plugin/gpu_plugin_object_factory.h"
#include "gpu/np_utils/np_utils.h"

namespace gpu_plugin {

GPUPluginObjectFactory::GPUPluginObjectFactory() {
}

GPUPluginObjectFactory::~GPUPluginObjectFactory() {
}

np_utils::PluginObject* GPUPluginObjectFactory::CreatePluginObject(
    NPP npp,
    NPMIMEType plugin_type) {
  if (strcmp(plugin_type, GPUPluginObject::kPluginType) == 0) {
    return np_utils::NPCreateObject<GPUPluginObject>(npp).ToReturned();
  }

  return NULL;
}

}  // namespace gpu_plugin
