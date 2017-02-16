// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/child_process_service_impl.h"

#include <android/native_window_jni.h>
#include <cpu-features.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/android/unguessable_token_android.h"
#include "base/file_descriptor_store.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/global_descriptors.h"
#include "base/unguessable_token.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "gpu/ipc/common/android/scoped_surface_request_conduit.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "ipc/ipc_descriptors.h"
#include "jni/ChildProcessServiceImpl_jni.h"
#include "services/service_manager/public/cpp/shared_file_util.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaParamRef;

namespace content {

namespace {

// TODO(sievers): Use two different implementations of this depending on if
// we're in a renderer or gpu process.
class ChildProcessSurfaceManager : public gpu::ScopedSurfaceRequestConduit,
                                   public gpu::GpuSurfaceLookup {
 public:
  ChildProcessSurfaceManager() {}
  ~ChildProcessSurfaceManager() override {}

  // |service impl| is the instance of
  // org.chromium.content.app.ChildProcessServiceImpl.
  void SetServiceImpl(const base::android::JavaRef<jobject>& service_impl) {
    service_impl_.Reset(service_impl);
  }

  // Overriden from ScopedSurfaceRequestConduit:
  void ForwardSurfaceTextureForSurfaceRequest(
      const base::UnguessableToken& request_token,
      const gl::SurfaceTexture* surface_texture) override {
    JNIEnv* env = base::android::AttachCurrentThread();

    content::
        Java_ChildProcessServiceImpl_forwardSurfaceTextureForSurfaceRequest(
            env, service_impl_,
            base::android::UnguessableTokenAndroid::Create(env, request_token),
            surface_texture->j_surface_texture());
  }

  // Overridden from GpuSurfaceLookup:
  gfx::AcceleratedWidget AcquireNativeWidget(int surface_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    gl::ScopedJavaSurface surface(
        content::Java_ChildProcessServiceImpl_getViewSurface(env, service_impl_,
                                                             surface_id));

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

  // Overridden from GpuSurfaceLookup:
  gl::ScopedJavaSurface AcquireJavaSurface(int surface_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    return gl::ScopedJavaSurface(
        content::Java_ChildProcessServiceImpl_getViewSurface(env, service_impl_,
                                                             surface_id));
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<ChildProcessSurfaceManager>;
  // The instance of org.chromium.content.app.ChildProcessServiceImpl.
  base::android::ScopedJavaGlobalRef<jobject> service_impl_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessSurfaceManager);
};

static base::LazyInstance<ChildProcessSurfaceManager>::Leaky
    g_child_process_surface_manager = LAZY_INSTANCE_INITIALIZER;

// Chrome actually uses the renderer code path for all of its child
// processes such as renderers, plugins, etc.
void InternalInitChildProcessImpl(JNIEnv* env,
                                  const JavaParamRef<jobject>& service_impl,
                                  jint cpu_count,
                                  jlong cpu_features) {
  // Set the CPU properties.
  android_setCpu(cpu_count, cpu_features);

  g_child_process_surface_manager.Get().SetServiceImpl(service_impl);

  gpu::GpuSurfaceLookup::InitInstance(
      g_child_process_surface_manager.Pointer());
  gpu::ScopedSurfaceRequestConduit::SetInstance(
      g_child_process_surface_manager.Pointer());

  base::android::MemoryPressureListenerAndroid::RegisterSystemCallback(env);
}

}  // namespace <anonymous>

void RegisterFileDescriptors(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             const JavaParamRef<jintArray>& j_ids,
                             const JavaParamRef<jintArray>& j_fds,
                             const JavaParamRef<jlongArray>& j_offsets,
                             const JavaParamRef<jlongArray>& j_sizes) {
  std::vector<int> ids;
  base::android::JavaIntArrayToIntVector(env, j_ids, &ids);
  std::vector<int> fds;
  base::android::JavaIntArrayToIntVector(env, j_fds, &fds);
  std::vector<int64_t> offsets;
  base::android::JavaLongArrayToInt64Vector(env, j_offsets, &offsets);
  std::vector<int64_t> sizes;
  base::android::JavaLongArrayToInt64Vector(env, j_sizes, &sizes);

  DCHECK_EQ(ids.size(), fds.size());
  DCHECK_EQ(fds.size(), offsets.size());
  DCHECK_EQ(offsets.size(), sizes.size());

  std::map<int, std::string> ids_to_keys;
  std::string file_switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSharedFiles);
  if (!file_switch_value.empty()) {
    base::Optional<std::map<int, std::string>> ids_to_keys_from_command_line =
        service_manager::ParseSharedFileSwitchValue(file_switch_value);
    if (ids_to_keys_from_command_line) {
      ids_to_keys = std::move(*ids_to_keys_from_command_line);
    }
  }

  for (size_t i = 0; i < ids.size(); i++) {
    base::MemoryMappedFile::Region region = {offsets.at(i), sizes.at(i)};
    int id = ids.at(i);
    int fd = fds.at(i);
    auto iter = ids_to_keys.find(id);
    if (iter != ids_to_keys.end()) {
      base::FileDescriptorStore::GetInstance().Set(iter->second,
                                                   base::ScopedFD(fd), region);
    } else {
      base::GlobalDescriptors::GetInstance()->Set(id, fd, region);
    }
  }
}

void InitChildProcessImpl(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          const JavaParamRef<jobject>& service_impl,
                          jint cpu_count,
                          jlong cpu_features) {
  InternalInitChildProcessImpl(env, service_impl, cpu_count, cpu_features);
}

void ExitChildProcess(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  VLOG(0) << "ChildProcessServiceImpl: Exiting child process.";
  base::android::LibraryLoaderExitHook();
  _exit(0);
}

bool RegisterChildProcessServiceImpl(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ShutdownMainThread(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  ChildThreadImpl::ShutdownThread();
}

}  // namespace content
