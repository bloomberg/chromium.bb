// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/child_process_service.h"

#include <android/native_window_jni.h>
#include <cpu-features.h>

#include "base/android/jni_array.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/logging.h"
#include "base/posix/global_descriptors.h"
#include "content/child/child_thread_impl.h"
#include "content/common/android/surface_texture_manager.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/common/gpu/gpu_surface_lookup.h"
#include "content/public/common/content_descriptors.h"
#include "ipc/ipc_descriptors.h"
#include "jni/ChildProcessService_jni.h"
#include "ui/gl/android/scoped_java_surface.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::JavaIntArrayToIntVector;

namespace content {

namespace {

// TODO(sievers): Use two different implementations of this depending on if
// we're in a renderer or gpu process.
class SurfaceTextureManagerImpl : public SurfaceTextureManager,
                                  public SurfaceTexturePeer,
                                  public GpuSurfaceLookup {
 public:
  // |service| is the instance of
  // org.chromium.content.app.ChildProcessService.
  explicit SurfaceTextureManagerImpl(
      const base::android::ScopedJavaLocalRef<jobject>& service)
      : service_(service) {
    SurfaceTexturePeer::InitInstance(this);
    GpuSurfaceLookup::InitInstance(this);
  }
  ~SurfaceTextureManagerImpl() override {
    SurfaceTexturePeer::InitInstance(NULL);
    GpuSurfaceLookup::InitInstance(NULL);
  }

  // Overridden from SurfaceTextureManager:
  void RegisterSurfaceTexture(int surface_texture_id,
                              int client_id,
                              gfx::SurfaceTexture* surface_texture) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ChildProcessService_createSurfaceTextureSurface(
        env,
        service_.obj(),
        surface_texture_id,
        client_id,
        surface_texture->j_surface_texture().obj());
  }
  void UnregisterSurfaceTexture(int surface_texture_id,
                                int client_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ChildProcessService_destroySurfaceTextureSurface(
        env, service_.obj(), surface_texture_id, client_id);
  }
  gfx::AcceleratedWidget AcquireNativeWidgetForSurfaceTexture(
      int surface_texture_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    gfx::ScopedJavaSurface surface(
        Java_ChildProcessService_getSurfaceTextureSurface(env, service_.obj(),
                                                          surface_texture_id));

    if (surface.j_surface().is_null())
      return NULL;

    // Note: This ensures that any local references used by
    // ANativeWindow_fromSurface are released immediately. This is needed as a
    // workaround for https://code.google.com/p/android/issues/detail?id=68174
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    ANativeWindow* native_window =
        ANativeWindow_fromSurface(env, surface.j_surface().obj());

    return native_window;
  }

  // Overridden from SurfaceTexturePeer:
  void EstablishSurfaceTexturePeer(
      base::ProcessHandle pid,
      scoped_refptr<gfx::SurfaceTexture> surface_texture,
      int primary_id,
      int secondary_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    content::Java_ChildProcessService_establishSurfaceTexturePeer(
        env,
        service_.obj(),
        pid,
        surface_texture->j_surface_texture().obj(),
        primary_id,
        secondary_id);
  }

  // Overridden from GpuSurfaceLookup:
  gfx::AcceleratedWidget AcquireNativeWidget(int surface_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    gfx::ScopedJavaSurface surface(
        content::Java_ChildProcessService_getViewSurface(
            env, service_.obj(), surface_id));

    if (surface.j_surface().is_null())
      return NULL;

    // Note: This ensures that any local references used by
    // ANativeWindow_fromSurface are released immediately. This is needed as a
    // workaround for https://code.google.com/p/android/issues/detail?id=68174
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    ANativeWindow* native_window =
        ANativeWindow_fromSurface(env, surface.j_surface().obj());

    return native_window;
  }

 private:
  // The instance of org.chromium.content.app.ChildProcessService.
  base::android::ScopedJavaGlobalRef<jobject> service_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTextureManagerImpl);
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

  SurfaceTextureManager::InitInstance(new SurfaceTextureManagerImpl(service));

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
  VLOG(0) << "ChildProcessService: Exiting child process.";
  base::android::LibraryLoaderExitHook();
  _exit(0);
}

bool RegisterChildProcessService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ShutdownMainThread(JNIEnv* env, jobject obj) {
  ChildThreadImpl::ShutdownThread();
}

}  // namespace content
