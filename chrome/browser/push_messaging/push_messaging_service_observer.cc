// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_service_observer.h"

#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "chrome/browser/push_messaging/push_messaging_service_observer_android.h"
#endif

// static
scoped_ptr<PushMessagingServiceObserver>
PushMessagingServiceObserver::Create() {
#if defined(OS_ANDROID)
  return make_scoped_ptr(new PushMessagingServiceObserverAndroid());
#endif
  return nullptr;
}
