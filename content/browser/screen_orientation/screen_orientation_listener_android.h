// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_LISTENER_ANDROID_H_
#define CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_LISTENER_ANDROID_H_

#include "base/macros.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "device/screen_orientation/public/interfaces/screen_orientation.mojom.h"

namespace content {

class ScreenOrientationListenerAndroid
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<
          device::mojom::ScreenOrientationListener>,
      public NON_EXPORTED_BASE(device::mojom::ScreenOrientationListener) {
 public:
  ScreenOrientationListenerAndroid();

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~ScreenOrientationListenerAndroid() override;

  // device::mojom::ScreenOrientationListener:
  void Start() override;
  void Stop() override;

  int listeners_count_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationListenerAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_LISTENER_ANDROID_H_
