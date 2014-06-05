// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_H_

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/public/platform/WebLockOrientationCallback.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationType.h"

namespace blink {
class WebScreenOrientationListener;
}

namespace content {

// ScreenOrientationDispatcher listens to message from the browser process and
// dispatch the orientation change ones to the WebScreenOrientationListener. It
// also does the bridge between the browser process and Blink with regards to
// lock orientation request and the handling of WebLockOrientationCallback.
class CONTENT_EXPORT ScreenOrientationDispatcher
    : public RenderProcessObserver {
 public:
  ScreenOrientationDispatcher();
  virtual ~ScreenOrientationDispatcher();

  // RenderProcessObserver
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  void setListener(blink::WebScreenOrientationListener* listener);

  // The |callback| is owned by ScreenOrientationDispatcher. It will be assigned
  // to |pending_callbacks_| that will delete it when the entry will be removed
  // from the map.
  void LockOrientation(blink::WebScreenOrientationLockType orientation,
                       scoped_ptr<blink::WebLockOrientationCallback> callback);

  void UnlockOrientation();

 private:
  void OnOrientationChange(blink::WebScreenOrientationType orientation);
  void OnLockSuccess(int request_id,
                     unsigned angle,
                     blink::WebScreenOrientationType orientation);
  void OnLockError(int request_id,
                   blink::WebLockOrientationCallback::ErrorType error);

  void CancelPendingLocks();

  blink::WebScreenOrientationListener* listener_;

  // The pending_callbacks_ map is mostly meant to have a unique ID to associate
  // with every callback going trough the dispatcher. The map will own the
  // pointer in the sense that it will destroy it when Remove() will be called.
  // Furthermore, we only expect to have one callback at a time in this map,
  // which is what IDMap was designed for.
  typedef IDMap<blink::WebLockOrientationCallback, IDMapOwnPointer> CallbackMap;
  CallbackMap pending_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationDispatcher);
};

}  // namespace content

#endif // CONTENT_RENDERER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_H_
