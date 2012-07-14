// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_INSTANCE_STATE_ACCESSOR_IMPL_H
#define CONTENT_RENDERER_PEPPER_PEPPER_INSTANCE_STATE_ACCESSOR_IMPL_H

#include "base/compiler_specific.h"
#include "content/renderer/pepper/pepper_instance_state_accessor.h"

namespace webkit {
namespace ppapi {
class PluginInstance;
class PluginModule;
}
}

namespace content {

class PepperInstanceStateAccessorImpl : public PepperInstanceStateAccessor {
 public:
  PepperInstanceStateAccessorImpl(webkit::ppapi::PluginModule* module);
  virtual ~PepperInstanceStateAccessorImpl();

  // PepperInstanceStateAccessor implmentation.
  virtual bool IsValidInstance(PP_Instance instance) OVERRIDE;
  virtual bool HasUserGesture(PP_Instance pp_instance) OVERRIDE;

 private:
  // Retrieves the plugin instance object associated with the given PP_Instance
  // and validates that it is one of the instances associated with our module.
  // Returns NULL on failure.
  //
  // We use this to security check the PP_Instance values sent from a plugin to
  // make sure it's not trying to spoof another instance.
  webkit::ppapi::PluginInstance* GetAndValidateInstance(PP_Instance instance);

  webkit::ppapi::PluginModule* module_;

  DISALLOW_COPY_AND_ASSIGN(PepperInstanceStateAccessorImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_INSTANCE_STATE_ACCESSOR_IMPL_H
