// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DELEGATE_PROVIDER_FACTORY_H_
#define DEVICE_VR_ANDROID_GVR_DELEGATE_PROVIDER_FACTORY_H_

#include "base/macros.h"

#include "device/vr/vr_export.h"

namespace device {

class GvrDelegateProvider;

class DEVICE_VR_EXPORT GvrDelegateProviderFactory {
 public:
  static GvrDelegateProvider* Create();
  static void Install(GvrDelegateProviderFactory* factory);

 protected:
  GvrDelegateProviderFactory() = default;
  virtual ~GvrDelegateProviderFactory() = default;

  virtual GvrDelegateProvider* CreateGvrDelegateProvider() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GvrDelegateProviderFactory);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DELEGATE_PROVIDER_FACTORY_H_
