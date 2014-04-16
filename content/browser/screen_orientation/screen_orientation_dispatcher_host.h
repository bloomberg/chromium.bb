// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_HOST_H_

#include "content/public/browser/browser_message_filter.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationType.h"

namespace content {

class ScreenOrientationProvider;

// ScreenOrientationDispatcherHost is a browser filter for Screen Orientation
// messages and also helps dispatching messages about orientation changes to the
// renderers.
class CONTENT_EXPORT ScreenOrientationDispatcherHost
    : public BrowserMessageFilter {
 public:
  ScreenOrientationDispatcherHost();

  // BrowserMessageFilter
  virtual bool OnMessageReceived(const IPC::Message&, bool*) OVERRIDE;

  void OnOrientationChange(blink::WebScreenOrientationType orientation);

  void SetProviderForTests(ScreenOrientationProvider* provider);

 private:
  virtual ~ScreenOrientationDispatcherHost();

  void OnLockRequest(blink::WebScreenOrientationLockType orientations);
  void OnUnlockRequest();

  static ScreenOrientationProvider* CreateProvider();

  scoped_ptr<ScreenOrientationProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationDispatcherHost);
};

}  // namespace content

#endif // CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_HOST_H_
