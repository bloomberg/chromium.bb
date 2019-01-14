// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_VR_MODULE_DELEGATE_H_
#define DEVICE_VR_ANDROID_GVR_VR_MODULE_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/vr_export.h"

namespace device {

// Delegates installation of the VR module.
class DEVICE_VR_EXPORT VrModuleDelegate {
 public:
  VrModuleDelegate() = default;
  virtual ~VrModuleDelegate() = default;
  // Returns true if the VR module is installed.
  virtual bool ModuleInstalled() = 0;
  // Asynchronously requests to install the VR module. |on_finished| is called
  // after the module install is completed. If |success| is false the module
  // install failed.
  virtual void InstallModule(
      base::OnceCallback<void(bool success)> on_finished) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VrModuleDelegate);
};

// Create a VR module delegate.
class DEVICE_VR_EXPORT VrModuleDelegateFactory {
 public:
  VrModuleDelegateFactory() = default;
  virtual ~VrModuleDelegateFactory() = default;
  // Returns the global module delegate factory.
  static VrModuleDelegateFactory* Get();
  // Sets the global module delegate factory.
  static void Set(std::unique_ptr<VrModuleDelegateFactory> factory);
  // Instantiates a VR module delegate. |render_process_id| and
  // |render_frame_id| refer to the tab in which to show module install UI.
  virtual std::unique_ptr<VrModuleDelegate> CreateDelegate(
      int render_process_id,
      int render_frame_id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VrModuleDelegateFactory);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_VR_MODULE_DELEGATE_H_
