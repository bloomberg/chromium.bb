// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ime_adapter_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/view_messages.h"
#include "jni/ImeAdapter_jni.h"

namespace content {

bool RegisterImeAdapter(JNIEnv* env) {
  if (!RegisterNativesImpl(env))
    return false;

  return true;
}

ImeAdapterAndroid::ImeAdapterAndroid(RenderWidgetHostViewAndroid* rwhva)
    : rwhva_(rwhva),
      java_ime_adapter_(NULL) {
}

ImeAdapterAndroid::~ImeAdapterAndroid() {
  if (java_ime_adapter_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ImeAdapter_detach(env, java_ime_adapter_);
    env->DeleteGlobalRef(java_ime_adapter_);
  }
}

void ImeAdapterAndroid::Unselect(JNIEnv* env, jobject) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_Unselect(rwhi->GetRoutingID()));
}

void ImeAdapterAndroid::SelectAll(JNIEnv* env, jobject) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_SelectAll(rwhi->GetRoutingID()));
}

void ImeAdapterAndroid::Cut(JNIEnv* env, jobject) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_Cut(rwhi->GetRoutingID()));
}

void ImeAdapterAndroid::Copy(JNIEnv* env, jobject) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_Copy(rwhi->GetRoutingID()));
}

void ImeAdapterAndroid::Paste(JNIEnv* env, jobject) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(
      rwhva_->GetRenderWidgetHost());
  if (!rwhi)
    return;

  rwhi->Send(new ViewMsg_Paste(rwhi->GetRoutingID()));
}

}  // namespace content