// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_BROWSER_TEST_MESSAGE_PUMP_ANDROID_
#define CONTENT_TEST_BROWSER_TEST_MESSAGE_PUMP_ANDROID_

#include "base/android/jni_android.h"
#include "base/compiler_specific.h"
#include "base/message_pump_android.h"

namespace content {

// MessgePump to be used for content browsertests on Android.
// This overrides the default ui message pump to allow nested loops.
class BrowserTestMessagePumpAndroid : public base::MessagePumpForUI {
 public:
  BrowserTestMessagePumpAndroid();

  virtual void Run(Delegate* delegate) OVERRIDE;
  virtual void Quit() OVERRIDE;
  virtual void ScheduleWork() OVERRIDE;
  virtual void ScheduleDelayedWork(
      const base::TimeTicks& delayed_work_time) OVERRIDE;
  virtual void Start(Delegate* delegate) OVERRIDE;

  static bool RegisterJni(JNIEnv* env);

 protected:
  virtual ~BrowserTestMessagePumpAndroid();

 private:
  // We may make recursive calls to Run, so we save state that needs to be
  // separate between them in this structure type.
  struct RunState;

  RunState* state_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTestMessagePumpAndroid);
};

}  // namespace content

#endif  // CONTENT_TEST_BROWSER_TEST_MESSAGE_PUMP_ANDROID_
