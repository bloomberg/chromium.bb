// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/screen_orientation_provider.h"

#include "base/callback_helpers.h"
#include "base/optional.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/screen_orientation_delegate.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebLockOrientationError.h"

namespace content {

using device::mojom::ScreenOrientationLockResult;

ScreenOrientationDelegate* ScreenOrientationProvider::delegate_ = nullptr;

ScreenOrientationProvider::ScreenOrientationProvider(WebContents* web_contents)
    : WebContentsObserver(web_contents), lock_applied_(false) {}

ScreenOrientationProvider::~ScreenOrientationProvider() {
}

void ScreenOrientationProvider::LockOrientation(
    blink::WebScreenOrientationLockType orientation,
    const LockOrientationCallback& callback) {
  // ScreenOrientation should have cancelled all pending request at this point.
  DCHECK(on_result_callback_.is_null());
  DCHECK(!pending_lock_orientation_.has_value());
  on_result_callback_ = callback;

  if (!delegate_ || !delegate_->ScreenOrientationProviderSupported()) {
    NotifyLockResult(ScreenOrientationLockResult::
                         SCREEN_ORIENTATION_LOCK_RESULT_ERROR_NOT_AVAILABLE);
    return;
  }

  if (delegate_->FullScreenRequired(web_contents())) {
    RenderViewHostImpl* rvhi =
        static_cast<RenderViewHostImpl*>(web_contents()->GetRenderViewHost());
    if (!rvhi) {
      NotifyLockResult(ScreenOrientationLockResult::
                           SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED);
      return;
    }
    if (!static_cast<WebContentsImpl*>(web_contents())
             ->IsFullscreenForCurrentTab()) {
      NotifyLockResult(
          ScreenOrientationLockResult::
              SCREEN_ORIENTATION_LOCK_RESULT_ERROR_FULLSCREEN_REQUIRED);
      return;
    }
  }

  if (orientation == blink::WebScreenOrientationLockNatural) {
    orientation = GetNaturalLockType();
    if (orientation == blink::WebScreenOrientationLockDefault) {
      // We are in a broken state, let's pretend we got canceled.
      NotifyLockResult(ScreenOrientationLockResult::
                           SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED);
      return;
    }
  }

  lock_applied_ = true;
  delegate_->Lock(web_contents(), orientation);

  // If the orientation we are locking to matches the current orientation, we
  // should succeed immediately.
  if (LockMatchesCurrentOrientation(orientation)) {
    NotifyLockResult(
        ScreenOrientationLockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);
    return;
  }

  pending_lock_orientation_ = orientation;
}

void ScreenOrientationProvider::UnlockOrientation() {
  if (!lock_applied_ || !delegate_)
    return;

  delegate_->Unlock(web_contents());

  lock_applied_ = false;
  pending_lock_orientation_.reset();
}

void ScreenOrientationProvider::OnOrientationChange() {
  if (!pending_lock_orientation_.has_value())
    return;

  if (LockMatchesCurrentOrientation(pending_lock_orientation_.value())) {
    DCHECK(!on_result_callback_.is_null());
    NotifyLockResult(
        ScreenOrientationLockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);
  }
}

void ScreenOrientationProvider::NotifyLockResult(
    ScreenOrientationLockResult result) {
  if (on_result_callback_.is_null()) {
    pending_lock_orientation_.reset();
    return;
  }
  base::ResetAndReturn(&on_result_callback_).Run(result);
  pending_lock_orientation_.reset();
}

void ScreenOrientationProvider::SetDelegate(
    ScreenOrientationDelegate* delegate) {
  delegate_ = delegate;
}

void ScreenOrientationProvider::DidToggleFullscreenModeForTab(
    bool entered_fullscreen, bool will_cause_resize) {
  if (!lock_applied_ || !delegate_)
    return;

  // If fullscreen is not required in order to lock orientation, don't unlock
  // when fullscreen state changes.
  if (!delegate_->FullScreenRequired(web_contents()))
    return;

  DCHECK(!entered_fullscreen);
  UnlockOrientation();
}

blink::WebScreenOrientationLockType
    ScreenOrientationProvider::GetNaturalLockType() const {
  RenderWidgetHost* rwh = web_contents()->GetRenderViewHost()->GetWidget();
  if (!rwh)
    return blink::WebScreenOrientationLockDefault;

  ScreenInfo screen_info;
  rwh->GetScreenInfo(&screen_info);

  switch (screen_info.orientation_type) {
    case SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY:
    case SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY:
      if (screen_info.orientation_angle == 0 ||
          screen_info.orientation_angle == 180) {
        return blink::WebScreenOrientationLockPortraitPrimary;
      }
      return blink::WebScreenOrientationLockLandscapePrimary;
    case SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY:
    case SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY:
      if (screen_info.orientation_angle == 0 ||
          screen_info.orientation_angle == 180) {
        return blink::WebScreenOrientationLockLandscapePrimary;
      }
      return blink::WebScreenOrientationLockPortraitPrimary;
    default:
      break;
  }

  NOTREACHED();
  return blink::WebScreenOrientationLockDefault;
}

bool ScreenOrientationProvider::LockMatchesCurrentOrientation(
    blink::WebScreenOrientationLockType lock) {
  RenderWidgetHost* rwh = web_contents()->GetRenderViewHost()->GetWidget();
  if (!rwh)
    return false;

  ScreenInfo screen_info;
  rwh->GetScreenInfo(&screen_info);

  switch (lock) {
    case blink::WebScreenOrientationLockPortraitPrimary:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
    case blink::WebScreenOrientationLockPortraitSecondary:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY;
    case blink::WebScreenOrientationLockLandscapePrimary:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY;
    case blink::WebScreenOrientationLockLandscapeSecondary:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY;
    case blink::WebScreenOrientationLockLandscape:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY ||
          screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY;
    case blink::WebScreenOrientationLockPortrait:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY ||
          screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY;
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

} // namespace content
