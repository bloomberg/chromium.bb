// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_DISPLAY_SCREEN_ORIENTATION_DELEGATE_CHROMEOS_H_
#define ASH_CONTENT_DISPLAY_SCREEN_ORIENTATION_DELEGATE_CHROMEOS_H_

#include "base/macros.h"
#include "content/public/browser/screen_orientation_delegate.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

namespace ash {

// Implements ChromeOS specific functionality for ScreenOrientationProvider.
class ScreenOrientationDelegate : public content::ScreenOrientationDelegate {
 public:
  ScreenOrientationDelegate();
  virtual ~ScreenOrientationDelegate();

  // content::ScreenOrientationDelegate:
  bool FullScreenRequired(content::WebContents* web_contents) override;
  void Lock(content::WebContents* web_contents,
            blink::WebScreenOrientationLockType lock_orientation) override;
  bool ScreenOrientationProviderSupported() override;
  void Unlock(content::WebContents* web_contents) override;

 private:
  // Locks rotation to the angle matching the primary orientation for
  // |lock_orientation|.
  void LockRotationToPrimaryOrientation(
      blink::WebScreenOrientationLockType lock_orientation);

  // Locks rotation to the angle matching the secondary orientation for
  // |lock_orientation|.
  void LockRotationToSecondaryOrientation(
      blink::WebScreenOrientationLockType lock_orientation);

  // For orientations that do not specify primary or secondary, locks to the
  // current rotation if it matches |lock_orientation|. Otherwise locks to a
  // matching rotation.
  void LockToRotationMatchingOrientation(
      blink::WebScreenOrientationLockType lock_orientation);

  // The window that has applied the current lock. No other window can apply a
  // lock until the current window unlocks rotation.
  aura::Window* locking_window_;

  // The orientation of the display when at a rotation of 0.
  blink::WebScreenOrientationLockType natural_orientation_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationDelegate);
};

}  // namespace ash

#endif  // ASH_CONTENT_DISPLAY_SCREEN_ORIENTATION_DELEGATE_CHROMEOS_H_
