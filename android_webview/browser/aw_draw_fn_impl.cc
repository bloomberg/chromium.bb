// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_draw_fn_impl.h"

#include "android_webview/public/browser/draw_gl.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "jni/AwDrawFnImpl_jni.h"

using base::android::JavaParamRef;
using content::BrowserThread;

namespace android_webview {

namespace {

AwDrawFnFunctionTable* g_draw_fn_function_table = nullptr;

void OnSyncWrapper(int functor, void* data, AwDrawFn_OnSyncParams* params) {
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->OnSync(params);
}

void OnContextDestroyedWrapper(int functor, void* data) {
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->OnContextDestroyed();
}

void OnDestroyedWrapper(int functor, void* data) {
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  delete static_cast<AwDrawFnImpl*>(data);
}

void DrawGLWrapper(int functor, void* data, AwDrawFn_DrawGLParams* params) {
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->DrawGL(params);
}

void InitVkWrapper(int functor, void* data, AwDrawFn_InitVkParams* params) {
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->InitVk(params);
}

void DrawVkWrapper(int functor, void* data, AwDrawFn_DrawVkParams* params) {
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->DrawVk(params);
}

void PostDrawVkWrapper(int functor,
                       void* data,
                       AwDrawFn_PostDrawVkParams* params) {
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->PostDrawVk(params);
}

}  // namespace

static void JNI_AwDrawFnImpl_SetDrawFnFunctionTable(JNIEnv* env,
                                                    jlong function_table) {
  g_draw_fn_function_table =
      reinterpret_cast<AwDrawFnFunctionTable*>(function_table);
}

AwDrawFnImpl::AwDrawFnImpl()
    : render_thread_manager_(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(g_draw_fn_function_table);

  static AwDrawFnFunctorCallbacks g_functor_callbacks{
      &OnSyncWrapper,      &OnContextDestroyedWrapper,
      &OnDestroyedWrapper, &DrawGLWrapper,
      &InitVkWrapper,      &DrawVkWrapper,
      &PostDrawVkWrapper,
  };

  functor_handle_ =
      g_draw_fn_function_table->create_functor(this, &g_functor_callbacks);
}

AwDrawFnImpl::~AwDrawFnImpl() {}

void AwDrawFnImpl::ReleaseHandle(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  render_thread_manager_.RemoveFromCompositorFrameProducerOnUI();
  g_draw_fn_function_table->release_functor(functor_handle_);
}

jint AwDrawFnImpl::GetFunctorHandle(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return functor_handle_;
}

jlong AwDrawFnImpl::GetCompositorFrameConsumer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(GetCompositorFrameConsumer());
}

static jlong JNI_AwDrawFnImpl_Create(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(new AwDrawFnImpl());
}

void AwDrawFnImpl::OnSync(AwDrawFn_OnSyncParams* params) {
  render_thread_manager_.CommitFrameOnRT();
}

void AwDrawFnImpl::OnContextDestroyed() {
  RenderThreadManager::InsideHardwareReleaseReset release_reset(
      &render_thread_manager_);
  render_thread_manager_.DestroyHardwareRendererOnRT(false /* save_restore */);
}

void AwDrawFnImpl::DrawGL(AwDrawFn_DrawGLParams* params) {
  struct HardwareRendererDrawParams hr_params {};
  hr_params.clip_left = params->clip_left;
  hr_params.clip_top = params->clip_top;
  hr_params.clip_right = params->clip_right;
  hr_params.clip_bottom = params->clip_bottom;
  hr_params.width = params->width;
  hr_params.height = params->height;
  hr_params.is_layer = params->is_layer;

  static_assert(base::size(decltype(params->transform){}) ==
                    base::size(hr_params.transform),
                "transform size mismatch");
  for (size_t i = 0; i < base::size(hr_params.transform); ++i) {
    hr_params.transform[i] = params->transform[i];
  }
  render_thread_manager_.DrawOnRT(false /* save_restore */, &hr_params);
}

void AwDrawFnImpl::InitVk(AwDrawFn_InitVkParams* params) {}

void AwDrawFnImpl::DrawVk(AwDrawFn_DrawVkParams* params) {}

void AwDrawFnImpl::PostDrawVk(AwDrawFn_PostDrawVkParams* params) {}

}  // namespace android_webview
