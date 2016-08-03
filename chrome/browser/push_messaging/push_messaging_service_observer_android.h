// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_OBSERVER_ANDROID_H_

#include "chrome/browser/push_messaging/push_messaging_service_observer.h"

#include <jni.h>

#include "base/macros.h"

// Observer for the PushMessagingService to be used on Android for forwarding
// message handled events to listeners in Java.
class PushMessagingServiceObserverAndroid
    : public PushMessagingServiceObserver {
 public:
  // PushMessagingServiceObserver implementation.
  void OnMessageHandled() override;

 private:
  friend class PushMessagingServiceObserver;
  PushMessagingServiceObserverAndroid() {}

  DISALLOW_COPY_AND_ASSIGN(PushMessagingServiceObserverAndroid);
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_OBSERVER_ANDROID_H_
