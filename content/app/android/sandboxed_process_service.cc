// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/sandboxed_process_service.h"

#include "base/global_descriptors_posix.h"
#include "base/logging.h"
#include "content/common/android/surface_texture_peer.h"
#if !defined(ANDROID_UPSTREAM_BRINGUP)
#include "content/common/chrome_descriptors.h"
#endif
#include "content/public/app/android_library_loader_hooks.h"
#include "ipc/ipc_descriptors.h"
#include "jni/sandboxed_process_service_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;

namespace {

class SurfaceTexturePeerSandboxedImpl : public content::SurfaceTexturePeer {
 public:
  // |service| is the instance of
  // org.chromium.content.app.SandboxedProcessService.
  SurfaceTexturePeerSandboxedImpl(jobject service)
      : service_(service) {
  }

  virtual ~SurfaceTexturePeerSandboxedImpl() {
  }

  virtual void EstablishSurfaceTexturePeer(base::ProcessHandle pid,
                                           SurfaceTextureTarget type,
                                           jobject j_surface_texture,
                                           int primary_id,
                                           int secondary_id) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_SandboxedProcessService_establishSurfaceTexturePeer(env, service_,
        pid, type, j_surface_texture, primary_id, secondary_id);
    CheckException(env);
  }

 private:
  // The instance of org.chromium.content.app.SandboxedProcessService.
  jobject service_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTexturePeerSandboxedImpl);
};

// Chrome actually uses the renderer code path for all of its sandboxed
// processes such as renderers, plugins, etc.
void InternalInitSandboxedProcess(int ipc_fd,
                                  int crash_fd,
                                  JNIEnv* env,
                                  jclass clazz,
                                  jobject context,
                                  jobject service) {
  // Set up the IPC file descriptor mapping.
  base::GlobalDescriptors::GetInstance()->Set(kPrimaryIPCChannel, ipc_fd);
#if defined(USE_LINUX_BREAKPAD)
  if (crash_fd > 0) {
    base::GlobalDescriptors::GetInstance()->Set(kCrashDumpSignal, crash_fd);
  }
#endif
  content::SurfaceTexturePeer::InitInstance(
      new SurfaceTexturePeerSandboxedImpl(service));

}

}  // namespace <anonymous>

static void InitSandboxedProcess(JNIEnv* env,
                                 jclass clazz,
                                 jobject context,
                                 jobject service,
                                 jint ipc_fd,
                                 jint crash_fd) {
  InternalInitSandboxedProcess(static_cast<int>(ipc_fd),
      static_cast<int>(crash_fd), env, clazz, context, service);

  // sandboxed process can't be reused. There is no need to wait for the browser
  // to unbind the service. Just exit and done.
  LOG(INFO) << "SandboxedProcessService: Drop out of SandboxedProcessMain.";
}

static void ExitSandboxedProcess(JNIEnv* env, jclass clazz) {
  LOG(INFO) << "SandboxedProcessService: Exiting sandboxed process.";
  // TODO(tedchoc): These methods should also be in the content namespace to
  // avoid specifying it in the LibraryLoaderExitHook call.
  content::LibraryLoaderExitHook();
  _exit(0);
}

namespace content {

bool RegisterSandboxedProcessService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
