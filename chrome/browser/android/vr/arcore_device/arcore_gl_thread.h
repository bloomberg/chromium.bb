// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_GL_THREAD_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_GL_THREAD_H_

#include <memory>
#include "base/android/java_handler_thread.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"

namespace device {

class ARCoreDevice;
class ARCoreGl;

class ARCoreGlThread : public base::android::JavaHandlerThread {
 public:
  ARCoreGlThread(
      ARCoreDevice* arcore_device,
      std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~ARCoreGlThread() override;
  ARCoreGl* GetARCoreGl();

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  ARCoreDevice* arcore_device_;
  std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge_;

  // Created on GL thread.
  std::unique_ptr<ARCoreGl> arcore_gl_;
  // This state is used for initializing arcore_gl.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  DISALLOW_COPY_AND_ASSIGN(ARCoreGlThread);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_GL_THREAD_H_
