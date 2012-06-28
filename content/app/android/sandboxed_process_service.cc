// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/sandboxed_process_service.h"

#include "base/android/jni_array.h"
#include "base/global_descriptors_posix.h"
#include "base/logging.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/public/app/android_library_loader_hooks.h"
#include "content/public/common/content_descriptors.h"
#include "ipc/ipc_descriptors.h"
#include "jni/sandboxed_process_service_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::JavaIntArrayToIntVector;

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
    content::Java_SandboxedProcessService_establishSurfaceTexturePeer(
        env, service_, pid, type, j_surface_texture, primary_id, secondary_id);
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
                                  const std::vector<int>& extra_file_ids,
                                  const std::vector<int>& extra_file_fds,
                                  JNIEnv* env,
                                  jclass clazz,
                                  jobject context,
                                  jobject service) {
  // Set up the IPC file descriptor mapping.
  base::GlobalDescriptors::GetInstance()->Set(kPrimaryIPCChannel, ipc_fd);
  // Register the extra file descriptors.
  // This usually include the crash dump signals and resource related files.
  DCHECK(extra_file_fds.size() == extra_file_ids.size());
  for (size_t i = 0; i < extra_file_ids.size(); ++i) {
    base::GlobalDescriptors::GetInstance()->Set(extra_file_ids[i],
                                                extra_file_fds[i]);
  }

  content::SurfaceTexturePeer::InitInstance(
      new SurfaceTexturePeerSandboxedImpl(service));

}

}  // namespace <anonymous>

namespace content {

void InitSandboxedProcess(JNIEnv* env,
                          jclass clazz,
                          jobject context,
                          jobject service,
                          jint ipc_fd,
                          jintArray j_extra_file_ids,
                          jintArray j_extra_file_fds) {
  std::vector<int> extra_file_ids;
  std::vector<int> extra_file_fds;
  JavaIntArrayToIntVector(env, j_extra_file_ids, &extra_file_ids);
  JavaIntArrayToIntVector(env, j_extra_file_fds, &extra_file_fds);

  InternalInitSandboxedProcess(static_cast<int>(ipc_fd),
                               extra_file_ids, extra_file_fds,
                               env, clazz, context, service);
}

void ExitSandboxedProcess(JNIEnv* env, jclass clazz) {
  LOG(INFO) << "SandboxedProcessService: Exiting sandboxed process.";
  LibraryLoaderExitHook();
  _exit(0);
}

bool RegisterSandboxedProcessService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
