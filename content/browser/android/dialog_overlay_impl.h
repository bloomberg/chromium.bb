// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_DIALOG_OVERLAY_IMPL_H_
#define CONTENT_BROWSER_ANDROID_DIALOG_OVERLAY_IMPL_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/unguessable_token.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/content_view_core_impl_observer.h"

namespace content {

// Native counterpart to DialogOverlayImpl java class.  This is created by the
// java side.  When the ContentViewCore for the provided token is attached or
// detached from a WindowAndroid, we get the Android window token and notify the
// java side.
class DialogOverlayImpl : public ContentViewCoreImplObserver {
 public:
  // Registers the JNI methods for DialogOverlayImpl.
  static bool RegisterDialogOverlayImpl(JNIEnv* env);

  DialogOverlayImpl(const base::android::JavaParamRef<jobject>& obj,
                    const base::UnguessableToken& token);
  ~DialogOverlayImpl() override;

  // Clean up and post to delete |this| later.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // ContentViewCoreImplObserver
  void OnContentViewCoreDestroyed() override;
  void OnAttachedToWindow() override;
  void OnDetachedFromWindow() override;

  // Unregister for tokens if we're registered, and clear |cvc_|.
  void UnregisterForTokensIfNeeded();

 private:
  // Look up the ContentViewCore for |renderer_pid_| and |render_frame_id_|.
  ContentViewCoreImpl* GetContentViewCore();

  // Java object that owns us.
  JavaObjectWeakGlobalRef obj_;

  base::UnguessableToken token_;

  // ContentViewCoreImpl instance that we're registered with as an observer.
  ContentViewCoreImpl* cvc_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_DIALOG_OVERLAY_IMPL_H_
