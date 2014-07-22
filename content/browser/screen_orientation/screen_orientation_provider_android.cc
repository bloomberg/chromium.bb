// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_provider_android.h"

#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/screen_orientation/screen_orientation_dispatcher_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "jni/ScreenOrientationProvider_jni.h"
#include "third_party/WebKit/public/platform/WebLockOrientationError.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"

namespace content {

ScreenOrientationProviderAndroid::LockInformation::LockInformation(
    int request_id, blink::WebScreenOrientationLockType lock)
    : request_id(request_id), lock(lock) {}

ScreenOrientationProviderAndroid::ScreenOrientationProviderAndroid(
    ScreenOrientationDispatcherHost* dispatcher,
    WebContents* web_contents)
    : ScreenOrientationProvider(),
      WebContentsObserver(web_contents),
      dispatcher_(dispatcher),
      lock_applied_(false),
      pending_lock_(NULL) {
}

ScreenOrientationProviderAndroid::~ScreenOrientationProviderAndroid() {
  if (pending_lock_)
    delete pending_lock_;
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
    blink::WebScreenOrientationLockType lock_orientation) {
  ContentViewCoreImpl* cvc =
      ContentViewCoreImpl::FromWebContents(web_contents());
  bool fullscreen_required = cvc ? cvc->IsFullscreenRequiredForOrientationLock()
                                 : true;

  if (fullscreen_required &&
      !web_contents_impl()->IsFullscreenForCurrentTab()) {
    dispatcher_->NotifyLockError(
        request_id,
        blink::WebLockOrientationErrorFullScreenRequired);
    return;
  }

  if (lock_orientation == blink::WebScreenOrientationLockNatural) {
    lock_orientation = GetNaturalLockType();
    if (lock_orientation == blink::WebScreenOrientationLockDefault) {
      // We are in a broken state, let's pretend we got canceled.
      dispatcher_->NotifyLockError(request_id,
                                   blink::WebLockOrientationErrorCanceled);
      return;
    }
  }

  if (j_screen_orientation_provider_.is_null()) {
    j_screen_orientation_provider_.Reset(Java_ScreenOrientationProvider_create(
        base::android::AttachCurrentThread()));
  }

  lock_applied_ = true;
  Java_ScreenOrientationProvider_lockOrientation(
      base::android::AttachCurrentThread(),
      j_screen_orientation_provider_.obj(), lock_orientation);

  // If two calls happen close to each other, Android will ignore the first.
  if (pending_lock_) {
    delete pending_lock_;
    pending_lock_ = NULL;
  }

  // If the orientation we are locking to matches the current orientation, we
  // should succeed immediately.
  if (LockMatchesCurrentOrientation(lock_orientation)) {
    dispatcher_->NotifyLockSuccess(request_id);
    return;
  }

  pending_lock_ = new LockInformation(request_id, lock_orientation);
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

void ScreenOrientationProviderAndroid::OnOrientationChange() {
  if (!pending_lock_)
    return;

  if (LockMatchesCurrentOrientation(pending_lock_->lock)) {
    dispatcher_->NotifyLockSuccess(pending_lock_->request_id);
    delete pending_lock_;
    pending_lock_ = NULL;
  }
}

void ScreenOrientationProviderAndroid::DidToggleFullscreenModeForTab(
    bool entered_fullscreen) {
  if (!lock_applied_)
    return;

  // If fullscreen is not required in order to lock orientation, don't unlock
  // when fullscreen state changes.
  ContentViewCoreImpl* cvc =
      ContentViewCoreImpl::FromWebContents(web_contents());
  if (cvc && !cvc->IsFullscreenRequiredForOrientationLock())
    return;

  DCHECK(!entered_fullscreen);
  UnlockOrientation();
}

bool ScreenOrientationProviderAndroid::LockMatchesCurrentOrientation(
    blink::WebScreenOrientationLockType lock_orientation) {
  if (!web_contents()->GetRenderViewHost())
    return false;

  RenderWidgetHost* rwh = web_contents()->GetRenderViewHost();
  blink::WebScreenInfo screen_info;
  rwh->GetWebScreenInfo(&screen_info);

  switch (lock_orientation) {
  case blink::WebScreenOrientationLockPortraitPrimary:
    return screen_info.orientationType ==
        blink::WebScreenOrientationPortraitPrimary;
  case blink::WebScreenOrientationLockPortraitSecondary:
    return screen_info.orientationType ==
        blink::WebScreenOrientationPortraitSecondary;
  case blink::WebScreenOrientationLockLandscapePrimary:
    return screen_info.orientationType ==
        blink::WebScreenOrientationLandscapePrimary;
  case blink::WebScreenOrientationLockLandscapeSecondary:
    return screen_info.orientationType ==
        blink::WebScreenOrientationLandscapeSecondary;
  case blink::WebScreenOrientationLockLandscape:
    return screen_info.orientationType ==
        blink::WebScreenOrientationLandscapePrimary ||
        screen_info.orientationType ==
        blink::WebScreenOrientationLandscapeSecondary;
  case blink::WebScreenOrientationLockPortrait:
    return screen_info.orientationType ==
        blink::WebScreenOrientationPortraitPrimary ||
        screen_info.orientationType ==
        blink::WebScreenOrientationPortraitSecondary;
  case blink::WebScreenOrientationLockAny:
    return true;
  case blink::WebScreenOrientationLockNatural:
  case blink::WebScreenOrientationLockDefault:
    NOTREACHED();
    return false;
  }

  NOTREACHED();
  return false;
}

blink::WebScreenOrientationLockType
ScreenOrientationProviderAndroid::GetNaturalLockType() const {
  if (!web_contents()->GetRenderViewHost())
    return blink::WebScreenOrientationLockDefault;

  RenderWidgetHost* rwh = web_contents()->GetRenderViewHost();
  blink::WebScreenInfo screen_info;
  rwh->GetWebScreenInfo(&screen_info);

  switch (screen_info.orientationType) {
  case blink::WebScreenOrientationPortraitPrimary:
  case blink::WebScreenOrientationPortraitSecondary:
    if (screen_info.orientationAngle == 0 ||
        screen_info.orientationAngle == 180) {
      return blink::WebScreenOrientationLockPortraitPrimary;
    }
    return blink::WebScreenOrientationLockLandscapePrimary;
  case blink::WebScreenOrientationLandscapePrimary:
  case blink::WebScreenOrientationLandscapeSecondary:
    if (screen_info.orientationAngle == 0 ||
        screen_info.orientationAngle == 180) {
      return blink::WebScreenOrientationLockLandscapePrimary;
    }
    return blink::WebScreenOrientationLockPortraitPrimary;
  case blink::WebScreenOrientationUndefined:
    NOTREACHED();
    return blink::WebScreenOrientationLockDefault;
  }

  NOTREACHED();
  return blink::WebScreenOrientationLockDefault;
}

// static
ScreenOrientationProvider* ScreenOrientationProvider::Create(
    ScreenOrientationDispatcherHost* dispatcher,
    WebContents* web_contents) {
  return new ScreenOrientationProviderAndroid(dispatcher, web_contents);
}

} // namespace content
