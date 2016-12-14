// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_H
#define CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_H

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/screen_orientation/public/interfaces/screen_orientation.mojom.h"

namespace content {

class ScreenOrientationProvider;
class WebContents;

class ScreenOrientation : public device::mojom::ScreenOrientation,
                          public WebContentsObserver {
 public:
  explicit ScreenOrientation(WebContents* web_contents);
  ~ScreenOrientation() override;

  ScreenOrientationProvider* GetScreenOrientationProvider();

 private:
  // ScreenOrientation:
  void LockOrientation(blink::WebScreenOrientationLockType orientation,
                       const LockOrientationCallback& callback) override;
  void UnlockOrientation() override;

  // WebContentsObserver:
  void DidNavigateMainFrame(const LoadCommittedDetails& details,
                            const FrameNavigateParams& params) override;

  void NotifyLockResult(device::mojom::ScreenOrientationLockResult result);

  std::unique_ptr<ScreenOrientationProvider> provider_;
  LockOrientationCallback on_result_callback_;
  WebContentsFrameBindingSet<device::mojom::ScreenOrientation> bindings_;
  base::WeakPtrFactory<ScreenOrientation> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_H
