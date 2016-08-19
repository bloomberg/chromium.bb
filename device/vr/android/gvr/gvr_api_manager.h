// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_API_MANAGER_H
#define DEVICE_VR_ANDROID_GVR_API_MANAGER_H

#include "device/vr/vr_export.h"

#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr.h"

namespace device {

class DEVICE_VR_EXPORT GvrApiManagerClient {
 public:
  virtual void OnGvrApiInitialized(gvr::GvrApi* gvr_api) = 0;
  virtual void OnGvrApiShutdown() = 0;
};

class DEVICE_VR_EXPORT GvrApiManager {
 public:
  static GvrApiManager* GetInstance();

  void AddClient(GvrApiManagerClient* client);
  void RemoveClient(GvrApiManagerClient* client);

  void Initialize(gvr_context* context);
  void Shutdown();

  gvr::GvrApi* gvr_api();

 private:
  GvrApiManager();
  ~GvrApiManager();

  std::unique_ptr<gvr::GvrApi> gvr_api_;

  using ClientList = std::vector<GvrApiManagerClient*>;
  ClientList clients_;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_API_MANAGER_H
