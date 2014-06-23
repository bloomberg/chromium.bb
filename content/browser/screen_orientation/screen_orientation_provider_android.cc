// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_provider_android.h"

#include "content/browser/screen_orientation/screen_orientation_dispatcher_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "jni/ScreenOrientationProvider_jni.h"
#include "third_party/WebKit/public/platform/WebLockOrientationError.h"

namespace content {

ScreenOrientationProviderAndroid::ScreenOrientationProviderAndroid(
    ScreenOrientationDispatcherHost* dispatcher,
    WebContents* web_contents)
    : ScreenOrientationProvider(),
      WebContentsObserver(web_contents),
      dispatcher_(dispatcher),
      lock_applied_(false) {
}

ScreenOrientationProviderAndroid::~ScreenOrientationProviderAndroid() {
}

WebContentsImpl* ScreenOrientationProviderAndroid::web_contents_impl() {
  return static_cast<WebContentsImpl*>(web_contents());
}

// static
bool ScreenOrientationProviderAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ScreenOrientationProviderAndroid::LockOrientation(
    int request_id,
    blink::WebScreenOrientationLockType orientation) {
  if (!web_contents_impl()->IsFullscreenForCurrentTab()) {
    dispatcher_->NotifyLockError(
        request_id,
        blink::WebLockOrientationErrorFullScreenRequired);
    return;
  }

  if (j_screen_orientation_provider_.is_null()) {
    j_screen_orientation_provider_.Reset(Java_ScreenOrientationProvider_create(
        base::android::AttachCurrentThread()));
  }

  lock_applied_ = true;
  Java_ScreenOrientationProvider_lockOrientation(
      base::android::AttachCurrentThread(),
      j_screen_orientation_provider_.obj(), orientation);
}

void ScreenOrientationProviderAndroid::UnlockOrientation() {
  if (!lock_applied_)
    return;

  // j_screen_orientation_provider_ was set when locking so it can't be null.
  DCHECK(!j_screen_orientation_provider_.is_null());

  Java_ScreenOrientationProvider_unlockOrientation(
      base::android::AttachCurrentThread(),
      j_screen_orientation_provider_.obj());
  lock_applied_ = false;
}

void ScreenOrientationProviderAndroid::DidToggleFullscreenModeForTab(
    bool entered_fullscreen) {
  if (lock_applied_) {
    DCHECK(!entered_fullscreen);
    UnlockOrientation();
  }
}

// static
ScreenOrientationProvider* ScreenOrientationProvider::Create(
    ScreenOrientationDispatcherHost* dispatcher,
    WebContents* web_contents) {
  return new ScreenOrientationProviderAndroid(dispatcher, web_contents);
}

} // namespace content
