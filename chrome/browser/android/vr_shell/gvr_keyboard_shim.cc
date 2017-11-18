// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/gvr-android-keyboard/src/libraries/headers/vr/gvr/capi/include/gvr_keyboard.h"

#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <jni.h>
#include <cmath>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "jni/GvrKeyboardLoaderClient_jni.h"

// Run CALL macro for every function defined in the API.
#define FOR_EACH_API_FN                                         \
  CALL(gvr_keyboard_initialize)                                 \
  CALL(gvr_keyboard_create)                                     \
  CALL(gvr_keyboard_get_input_mode)                             \
  CALL(gvr_keyboard_set_input_mode)                             \
  CALL(gvr_keyboard_get_recommended_world_from_keyboard_matrix) \
  CALL(gvr_keyboard_set_world_from_keyboard_matrix)             \
  CALL(gvr_keyboard_show)                                       \
  CALL(gvr_keyboard_update_button_state)                        \
  CALL(gvr_keyboard_update_controller_ray)                      \
  CALL(gvr_keyboard_get_text)                                   \
  CALL(gvr_keyboard_set_text)                                   \
  CALL(gvr_keyboard_get_selection_indices)                      \
  CALL(gvr_keyboard_set_selection_indices)                      \
  CALL(gvr_keyboard_get_composing_indices)                      \
  CALL(gvr_keyboard_set_composing_indices)                      \
  CALL(gvr_keyboard_set_frame_time)                             \
  CALL(gvr_keyboard_set_eye_from_world_matrix)                  \
  CALL(gvr_keyboard_set_projection_matrix)                      \
  CALL(gvr_keyboard_set_viewport)                               \
  CALL(gvr_keyboard_advance_frame)                              \
  CALL(gvr_keyboard_render)                                     \
  CALL(gvr_keyboard_hide)                                       \
  CALL(gvr_keyboard_destroy)

namespace {

// Declare implementation function pointers.
#define CALL(fn) decltype(&fn) impl_##fn = nullptr;
FOR_EACH_API_FN
#undef CALL

template <typename Fn>
bool LoadFunction(void* handle, const char* function_name, Fn* fn_out) {
  void* fn = dlsym(handle, function_name);
  if (!fn) {
    LOG(ERROR) << "Failed to load " << function_name
               << " from GVR keyboard library: " << dlerror();
    return false;
  }
  *fn_out = reinterpret_cast<Fn>(fn);
  return true;
}

static void* sdk_handle = nullptr;

void CloseSdk() {
  if (!sdk_handle)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  vr_shell::Java_GvrKeyboardLoaderClient_closeKeyboardSDK(
      env, reinterpret_cast<jlong>(sdk_handle));

// Null all the function pointers.
#define CALL(fn) impl_##fn = nullptr;
  FOR_EACH_API_FN
#undef CALL

  sdk_handle = nullptr;
}

bool LoadSdk(void* closure, gvr_keyboard_callback callback) {
  if (sdk_handle)
    return true;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  base::android::ScopedJavaLocalRef<jobject> context_wrapper =
      vr_shell::Java_GvrKeyboardLoaderClient_getContextWrapper(env);

  base::android::ScopedJavaLocalRef<jobject> remote_class_loader =
      vr_shell::Java_GvrKeyboardLoaderClient_getRemoteClassLoader(env);

  sdk_handle = reinterpret_cast<void*>(
      vr_shell::Java_GvrKeyboardLoaderClient_loadKeyboardSDK(env));

  if (!sdk_handle) {
    LOG(ERROR) << "Failed to load GVR keyboard SDK.";
    return false;
  }

  // Load all function pointers from the SDK.
  bool success = true;
#define CALL(fn) success &= LoadFunction(sdk_handle, #fn, &impl_##fn);
  FOR_EACH_API_FN
#undef CALL

  if (!success) {
    CloseSdk();
    return false;
  }

  gvr_keyboard_initialize(env, context_wrapper.obj(),
                          remote_class_loader.obj());

  return success;
}

}  // namespace

void gvr_keyboard_initialize(JNIEnv* env,
                             jobject app_context,
                             jobject class_loader) {
  impl_gvr_keyboard_initialize(env, app_context, class_loader);
}

gvr_keyboard_context* gvr_keyboard_create(void* closure,
                                          gvr_keyboard_callback callback) {
  if (!LoadSdk(closure, callback)) {
    return nullptr;
  }
  return impl_gvr_keyboard_create(closure, callback);
}

void gvr_keyboard_destroy(gvr_keyboard_context** context) {
  impl_gvr_keyboard_destroy(context);
  CloseSdk();
}

int32_t gvr_keyboard_get_input_mode(gvr_keyboard_context* context) {
  return impl_gvr_keyboard_get_input_mode(context);
}

void gvr_keyboard_set_input_mode(gvr_keyboard_context* context,
                                 int32_t input_mode) {
  impl_gvr_keyboard_set_input_mode(context, input_mode);
}

void gvr_keyboard_get_recommended_world_from_keyboard_matrix(
    float distance_from_eye,
    gvr_mat4f* matrix) {
  impl_gvr_keyboard_get_recommended_world_from_keyboard_matrix(
      distance_from_eye, matrix);
}

void gvr_keyboard_set_world_from_keyboard_matrix(gvr_keyboard_context* context,
                                                 const gvr_mat4f* matrix) {
  impl_gvr_keyboard_set_world_from_keyboard_matrix(context, matrix);
}

void gvr_keyboard_show(gvr_keyboard_context* context) {
  impl_gvr_keyboard_show(context);
}

void gvr_keyboard_update_button_state(gvr_keyboard_context* context,
                                      int32_t button_index,
                                      bool pressed) {
  impl_gvr_keyboard_update_button_state(context, button_index, pressed);
}

bool gvr_keyboard_update_controller_ray(gvr_keyboard_context* context,
                                        const gvr_vec3f* start,
                                        const gvr_vec3f* end,
                                        gvr_vec3f* hit) {
  return impl_gvr_keyboard_update_controller_ray(context, start, end, hit);
}

char* gvr_keyboard_get_text(gvr_keyboard_context* context) {
  return impl_gvr_keyboard_get_text(context);
}

void gvr_keyboard_set_text(gvr_keyboard_context* context, const char* text) {
  return impl_gvr_keyboard_set_text(context, text);
}

void gvr_keyboard_get_selection_indices(gvr_keyboard_context* context,
                                        size_t* start,
                                        size_t* end) {
  impl_gvr_keyboard_get_selection_indices(context, start, end);
}

void gvr_keyboard_set_selection_indices(gvr_keyboard_context* context,
                                        size_t start,
                                        size_t end) {
  impl_gvr_keyboard_set_selection_indices(context, start, end);
}

void gvr_keyboard_get_composing_indices(gvr_keyboard_context* context,
                                        size_t* start,
                                        size_t* end) {
  impl_gvr_keyboard_get_composing_indices(context, start, end);
}

void gvr_keyboard_set_composing_indices(gvr_keyboard_context* context,
                                        size_t start,
                                        size_t end) {
  impl_gvr_keyboard_set_composing_indices(context, start, end);
}

void gvr_keyboard_set_frame_time(gvr_keyboard_context* context,
                                 const gvr_clock_time_point* time) {
  impl_gvr_keyboard_set_frame_time(context, time);
}

void gvr_keyboard_set_eye_from_world_matrix(gvr_keyboard_context* context,
                                            int32_t eye_type,
                                            const gvr_mat4f* matrix) {
  impl_gvr_keyboard_set_eye_from_world_matrix(context, eye_type, matrix);
}

void gvr_keyboard_set_projection_matrix(gvr_keyboard_context* context,
                                        int32_t eye_type,
                                        const gvr_mat4f* projection) {
  impl_gvr_keyboard_set_projection_matrix(context, eye_type, projection);
}

void gvr_keyboard_set_viewport(gvr_keyboard_context* context,
                               int32_t eye_type,
                               const gvr_recti* viewport) {
  impl_gvr_keyboard_set_viewport(context, eye_type, viewport);
}

void gvr_keyboard_advance_frame(gvr_keyboard_context* context) {
  impl_gvr_keyboard_advance_frame(context);
}

void gvr_keyboard_render(gvr_keyboard_context* context, int32_t eye_type) {
  impl_gvr_keyboard_render(context, eye_type);
}

void gvr_keyboard_hide(gvr_keyboard_context* context) {
  impl_gvr_keyboard_hide(context);
}

#undef FOR_EACH_API_FN
