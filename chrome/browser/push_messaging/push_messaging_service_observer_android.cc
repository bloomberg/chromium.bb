// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_service_observer_android.h"

#include "base/android/jni_android.h"
#include "jni/PushMessagingServiceObserver_jni.h"

void PushMessagingServiceObserverAndroid::OnMessageHandled() {
  chrome::android::Java_PushMessagingServiceObserver_onMessageHandled(
      base::android::AttachCurrentThread());
}
