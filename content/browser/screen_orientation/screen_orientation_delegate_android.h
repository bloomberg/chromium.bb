// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DELEGATE_H_
#define CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DELEGATE_H_

#include <jni.h>

#include "base/macros.h"
#include "content/public/browser/screen_orientation_delegate.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"

namespace content {

class WebContents;

// Android implementation of ScreenOrientationDelegate. The functionality of
// ScreenOrientationProvider is always supported.
class ScreenOrientationDelegateAndroid : public ScreenOrientationDelegate {
 public:
  ScreenOrientationDelegateAndroid();
  virtual ~ScreenOrientationDelegateAndroid();

  static bool Register(JNIEnv* env);

  // Ask the ScreenOrientationListener (Java) to start accurately listening to
  // the screen orientation. It keep track of the number of start request if it
  // is already running an accurate listening.
  static void StartAccurateListening();

  // Ask the ScreenOrientationListener (Java) to stop accurately listening to
  // the screen orientation. It will actually stop only if the number of stop
  // requests matches the number of start requests.
  static void StopAccurateListening();

  // ScreenOrientationDelegate:
  virtual bool FullScreenRequired(WebContents* web_contents) override;
  virtual void Lock(
      blink::WebScreenOrientationLockType lock_orientation) override;
  virtual bool ScreenOrientationProviderSupported() override;
  virtual void Unlock() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationDelegateAndroid);
};

} // namespace content

#endif // CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DELEGATE_H_
