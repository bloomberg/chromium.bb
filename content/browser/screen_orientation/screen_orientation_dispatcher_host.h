// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SCREEN_ORIENTATION_DISPATCHER_HOST_H_

#include "content/public/browser/browser_message_filter.h"
#include "third_party/WebKit/public/platform/WebScreenOrientation.h"

namespace content {

// ScreenOrientationDispatcherHost
class ScreenOrientationDispatcherHost : public BrowserMessageFilter {
 public:
  ScreenOrientationDispatcherHost();

  // BrowserMessageFilter
  virtual bool OnMessageReceived(const IPC::Message&, bool*) OVERRIDE;

  void OnOrientationChange(blink::WebScreenOrientation orientation);

private:
  virtual ~ScreenOrientationDispatcherHost() {}

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationDispatcherHost);
};

}  // namespace content

#endif // CONTENT_BROWSER_SCREEN_ORIENTATION_DISPATCHER_HOST_H_
