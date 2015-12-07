// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WAKE_LOCK_WAKE_LOCK_DISPATCHER_H_
#define CONTENT_RENDERER_WAKE_LOCK_WAKE_LOCK_DISPATCHER_H_

#include "content/common/wake_lock_service.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/modules/wake_lock/WebWakeLockClient.h"

namespace content {

// WakeLockDispatcher sends wake lock messages to the browser process using the
// Mojo WakeLockService.
class WakeLockDispatcher : public RenderFrameObserver,
                           public blink::WebWakeLockClient {
 public:
  explicit WakeLockDispatcher(RenderFrame* render_frame);
  ~WakeLockDispatcher() override;

 private:
  // WebWakeLockClient implementation.
  void requestKeepScreenAwake(bool keepScreenAwake) override;

  WakeLockServicePtr wake_lock_service_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WAKE_LOCK_WAKE_LOCK_DISPATCHER_H_
