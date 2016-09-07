// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DELEGATE_H
#define DEVICE_VR_ANDROID_GVR_DELEGATE_H

#include "device/vr/vr_export.h"

#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr.h"

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}  // namespace base

namespace device {

class DEVICE_VR_EXPORT GvrDelegate {
 public:
  virtual void RequestWebVRPresent() = 0;
  virtual void ExitWebVRPresent() = 0;

  virtual void SubmitWebVRFrame() = 0;
  virtual void UpdateWebVRTextureBounds(int eye,
                                        float left,
                                        float top,
                                        float width,
                                        float height) = 0;

  virtual gvr::GvrApi* gvr_api() = 0;
};

class DEVICE_VR_EXPORT GvrDelegateClient {
 public:
  virtual void OnDelegateInitialized(GvrDelegate* delegate) = 0;
  virtual void OnDelegateShutdown() = 0;
};

class DEVICE_VR_EXPORT GvrDelegateManager {
 public:
  static GvrDelegateManager* GetInstance();

  void AddClient(GvrDelegateClient* client);
  void RemoveClient(GvrDelegateClient* client);

  void Initialize(GvrDelegate* delegate);
  void Shutdown();

  GvrDelegate* delegate() { return delegate_; }

 private:
  friend struct base::DefaultSingletonTraits<GvrDelegateManager>;

  GvrDelegateManager();
  ~GvrDelegateManager();

  GvrDelegate* delegate_;

  using ClientList = std::vector<GvrDelegateClient*>;
  ClientList clients_;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DELEGATE_H
