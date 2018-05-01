// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_gl_thread.h"

#include <utility>
#include "base/message_loop/message_loop.h"
#include "base/version.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device.h"
#include "chrome/browser/android/vr/arcore_device/arcore_gl.h"

namespace device {

ARCoreGlThread::ARCoreGlThread(
    ARCoreDevice* arcore_device,
    std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : base::android::JavaHandlerThread("ARCoreGL"),
      arcore_device_(arcore_device),
      main_thread_task_runner_(std::move(main_thread_task_runner)) {
  mailbox_bridge_ = std::move(mailbox_bridge);
}

ARCoreGlThread::~ARCoreGlThread() {
  Stop();
}

ARCoreGl* ARCoreGlThread::GetARCoreGl() {
  return arcore_gl_.get();
}

void ARCoreGlThread::Init() {
  arcore_gl_ = std::make_unique<ARCoreGl>(
      arcore_device_, base::ResetAndReturn(&mailbox_bridge_),
      main_thread_task_runner_);
  arcore_gl_->Initialize();
}

void ARCoreGlThread::CleanUp() {
  arcore_gl_.reset();
}

}  // namespace device
