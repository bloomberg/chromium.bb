// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_H_

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "device/screen_orientation/public/interfaces/screen_orientation.mojom.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebLockOrientationCallback.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationClient.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace content {

using device::mojom::ScreenOrientationAssociatedPtr;
using device::mojom::ScreenOrientationLockResult;

class RenderFrame;

// ScreenOrientationDispatcher implements the WebScreenOrientationClient API
// which handles screen lock. It sends lock (or unlock) requests to the browser
// process and listens for responses and let Blink know about the result of the
// request via WebLockOrientationCallback.
class CONTENT_EXPORT ScreenOrientationDispatcher
    : public RenderFrameObserver,
      public blink::WebScreenOrientationClient {
 public:
  explicit ScreenOrientationDispatcher(RenderFrame* render_frame);
  ~ScreenOrientationDispatcher() override;

 private:
  friend class ScreenOrientationDispatcherTest;

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // blink::WebScreenOrientationClient implementation.
  void LockOrientation(
      blink::WebScreenOrientationLockType orientation,
      std::unique_ptr<blink::WebLockOrientationCallback> callback) override;
  void UnlockOrientation() override;

  void OnLockOrientationResult(int request_id,
                               ScreenOrientationLockResult result);

  void CancelPendingLocks();

  int GetRequestIdForTests();

  void EnsureScreenOrientationService();

  // This should only be called by ScreenOrientationDispatcherTest
  void SetScreenOrientationForTests(
      ScreenOrientationAssociatedPtr& screen_orientation_for_tests) {
    screen_orientation_ = std::move(screen_orientation_for_tests);
  }

  std::unique_ptr<blink::WebLockOrientationCallback> pending_callback_;
  int request_id_ = 0;

  ScreenOrientationAssociatedPtr screen_orientation_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_H_
