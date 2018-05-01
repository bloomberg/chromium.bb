// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_

#include <jni.h>
#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_base.h"

namespace vr {
class MailboxToSurfaceBridge;
}  // namespace vr

namespace device {

class ARCoreGlThread;

class ARCoreDevice : public VRDeviceBase {
 public:
  ARCoreDevice();
  ~ARCoreDevice() override;

  mojom::VRPosePtr Update();
  void OnFrameData(mojom::VRMagicWindowFrameDataPtr frame_data,
                   mojom::VRMagicWindowProvider::GetFrameDataCallback callback);
  scoped_refptr<base::SingleThreadTaskRunner> getTaskRunner() {
    return main_thread_task_runner_;
  }

  vr::MailboxToSurfaceBridge* GetMailboxBridge() {
    return mailbox_bridge_.get();
  }

  base::WeakPtr<ARCoreDevice> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnMagicWindowFrameDataRequest(
      const gfx::Size& frame_size,
      display::Display::Rotation frame_rotation,
      mojom::VRMagicWindowProvider::GetFrameDataCallback callback) override;
  void OnMailboxBridgeReady();
  bool IsOnMainThread();
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge_;
  std::unique_ptr<ARCoreGlThread> arcore_gl_thread_;

  // Must be last.
  base::WeakPtrFactory<ARCoreDevice> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ARCoreDevice);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_
