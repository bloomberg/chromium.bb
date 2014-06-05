// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_H_

#include "base/macros.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationType.h"

namespace blink {
class WebScreenOrientationListener;
}

namespace content {

class RenderThread;

// ScreenOrientationDispatcher listens to message from the browser process and
// dispatch the orientation change ones to the WebScreenOrientationListener.
class CONTENT_EXPORT ScreenOrientationDispatcher
    : public RenderProcessObserver {
 public:
  explicit ScreenOrientationDispatcher(RenderThread*);
  virtual ~ScreenOrientationDispatcher() {}

  // RenderProcessObserver
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  void setListener(blink::WebScreenOrientationListener* listener);

 private:
  void OnOrientationChange(blink::WebScreenOrientationType orientation);

  blink::WebScreenOrientationListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationDispatcher);
};

}  // namespace content

#endif // CONTENT_RENDERER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_H_
