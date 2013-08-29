// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/child_process_service.h"

#include <android/native_window_jni.h>
#include <cpu-features.h>

#include "base/android/jni_array.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/logging.h"
#include "base/posix/global_descriptors.h"
#include "content/child/child_thread.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/common/gpu/gpu_surface_lookup.h"
#include "content/public/app/android_library_loader_hooks.h"
#include "content/public/common/content_descriptors.h"
#include "ipc/ipc_descriptors.h"
#include "jni/ChildProcessService_jni.h"
#include "ui/gl/android/scoped_java_surface.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::JavaIntArrayToIntVector;

namespace content {

namespace {

class SurfaceTexturePeerChildImpl : public content::SurfaceTexturePeer,
                                    public content::GpuSurfaceLookup {
 public:
  // |service| is the instance of
  // org.chromium.content.app.ChildProcessService.
  explicit SurfaceTexturePeerChildImpl(
      const base::android::ScopedJavaLocalRef<jobject>& service)
      : service_(service) {
    GpuSurfaceLookup::InitInstance(this);
  }

  virtual ~SurfaceTexturePeerChildImpl() {
    GpuSurfaceLookup::InitInstance(NULL);
  }

  virtual void EstablishSurfaceTexturePeer(
      base::ProcessHandle pid,
      scoped_refptr<gfx::SurfaceTexture> surface_texture,
      int primary_id,
      int secondary_id) OVERRIDE {
    JNIEnv* env = base::android::AttachCurrentThread();
    content::Java_ChildProcessService_establishSurfaceTexturePeer(
        env, service_.obj(), pid,
        surface_texture->j_surface_texture().obj(), primary_id,
        secondary_id);
    CheckException(env);
  }

  virtual gfx::AcceleratedWidget AcquireNativeWidget(int surface_id) OVERRIDE {
    JNIEnv* env = base::android::AttachCurrentThread();
    gfx::ScopedJavaSurface surface(
        content::Java_ChildProcessService_getViewSurface(
        env, service_.obj(), surface_id));

    if (surface.j_surface().is_null())
      return NULL;

    ANativeWindow* native_window = ANativeWindow_fromSurface(
        env, surface.j_surface().obj());

    return native_window;
  }

 private:
  // The instance of org.chromium.content.app.ChildProcessService.
  base::android::ScopedJavaGlobalRef<jobject> service_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTexturePeerChildImpl);
};

// Chrome actually uses the renderer code path for all of its child
// processes such as renderers, plugins, etc.
void InternalInitChildProcess(const std::vector<int>& file_ids,
                              const std::vector<int>& file_fds,
                              JNIEnv* env,
                              jclass clazz,
                              jobject context,
                              jobject service_in,
                              jint cpu_count,
                              jlong cpu_features) {
  base::android::ScopedJavaLocalRef<jobject> service(env, service_in);

  // Set the CPU properties.
  android_setCpu(cpu_count, cpu_features);
  // Register the file descriptors.
  // This includes the IPC channel, the crash dump signals and resource related
  // files.
  DCHECK(file_fds.size() == file_ids.size());
  for (size_t i = 0; i < file_ids.size(); ++i)
    base::GlobalDescriptors::GetInstance()->Set(file_ids[i], file_fds[i]);

  content::SurfaceTexturePeer::InitInstance(
      new SurfaceTexturePeerChildImpl(service));

  base::android::MemoryPressureListenerAndroid::RegisterSystemCallback(env);
}

}  // namespace <anonymous>

void InitChildProcess(JNIEnv* env,
                      jclass clazz,
                      jobject context,
                      jobject service,
                      jintArray j_file_ids,
                      jintArray j_file_fds,
                      jint cpu_count,
                      jlong cpu_features) {
  std::vector<int> file_ids;
  std::vector<int> file_fds;
  JavaIntArrayToIntVector(env, j_file_ids, &file_ids);
  JavaIntArrayToIntVector(env, j_file_fds, &file_fds);

  InternalInitChildProcess(
      file_ids, file_fds, env, clazz, context, service,
      cpu_count, cpu_features);
}

void ExitChildProcess(JNIEnv* env, jclass clazz) {
  LOG(INFO) << "ChildProcessService: Exiting child process.";
  LibraryLoaderExitHook();
  _exit(0);
}

bool RegisterChildProcessService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ShutdownMainThread(JNIEnv* env, jobject obj) {
  ChildThread::ShutdownThread();
}

}  // namespace content
