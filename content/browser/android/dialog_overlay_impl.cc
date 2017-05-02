// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/dialog_overlay_impl.h"

#include "content/public/browser/web_contents.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "jni/DialogOverlayImpl_jni.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

// static
bool DialogOverlayImpl::RegisterDialogOverlayImpl(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  jlong high,
                  jlong low) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<jlong>(new DialogOverlayImpl(
      obj, base::UnguessableToken::Deserialize(high, low)));
}

DialogOverlayImpl::DialogOverlayImpl(const JavaParamRef<jobject>& obj,
                                     const base::UnguessableToken& token)
    : token_(token), cvc_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cvc_ = GetContentViewCore();

  JNIEnv* env = AttachCurrentThread();
  obj_ = JavaObjectWeakGlobalRef(env, obj);

  // If there's no CVC, then just post a null token immediately.
  if (!cvc_) {
    Java_DialogOverlayImpl_onDismissed(env, obj.obj());
    return;
  }

  cvc_->AddObserver(this);

  // Also send the initial token, since we'll only get changes.
  if (ui::WindowAndroid* window = cvc_->GetWindowAndroid()) {
    ScopedJavaLocalRef<jobject> token = window->GetWindowToken();
    if (!token.is_null())
      Java_DialogOverlayImpl_onWindowToken(env, obj.obj(), token);
  }
}

DialogOverlayImpl::~DialogOverlayImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We should only be deleted after one unregisters for token callbacks.
  DCHECK(!cvc_);
}

void DialogOverlayImpl::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UnregisterForTokensIfNeeded();
  // We delete soon since this might be part of an onDismissed callback.
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
}

void DialogOverlayImpl::UnregisterForTokensIfNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!cvc_)
    return;

  cvc_->RemoveObserver(this);
  cvc_ = nullptr;
}

void DialogOverlayImpl::OnContentViewCoreDestroyed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cvc_ = nullptr;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = obj_.get(env);
  if (!obj.is_null())
    Java_DialogOverlayImpl_onDismissed(env, obj.obj());
}

void DialogOverlayImpl::OnAttachedToWindow() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> token;

  if (ui::WindowAndroid* window = cvc_->GetWindowAndroid())
    token = window->GetWindowToken();

  ScopedJavaLocalRef<jobject> obj = obj_.get(env);
  if (!obj.is_null())
    Java_DialogOverlayImpl_onWindowToken(env, obj.obj(), token);
}

void DialogOverlayImpl::OnDetachedFromWindow() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = obj_.get(env);
  if (!obj.is_null())
    Java_DialogOverlayImpl_onWindowToken(env, obj.obj(), nullptr);
}

ContentViewCoreImpl* DialogOverlayImpl::GetContentViewCore() {
  // Get the frame from the token.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderFrameHost* frame =
      content::RenderFrameHostImpl::FromOverlayRoutingToken(token_);
  if (!frame) {
    DVLOG(1) << "Cannot find frame host for token " << token_;
    return nullptr;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  DCHECK(web_contents);

  content::ContentViewCoreImpl* cvc =
      content::ContentViewCoreImpl::FromWebContents(web_contents);
  if (!cvc) {
    DVLOG(1) << "Cannot find cvc for token " << token_;
    return nullptr;
  }

  return cvc;
}

static jint RegisterSurface(JNIEnv* env,
                            const base::android::JavaParamRef<jclass>& jcaller,
                            const JavaParamRef<jobject>& surface) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return gpu::GpuSurfaceTracker::Get()->AddSurfaceForNativeWidget(
      gpu::GpuSurfaceTracker::SurfaceRecord(gfx::kNullAcceleratedWidget,
                                            surface.obj()));
}

static void UnregisterSurface(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    jint surface_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  gpu::GpuSurfaceTracker::Get()->RemoveSurface(surface_id);
}

}  // namespace content
