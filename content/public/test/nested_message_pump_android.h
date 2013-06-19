// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NESTED_MESSAGE_PUMP_ANDROID_
#define CONTENT_PUBLIC_TEST_NESTED_MESSAGE_PUMP_ANDROID_

#include "base/android/jni_android.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_pump_android.h"

namespace content {

// A nested message pump to be used for content browsertests and layout tests
// on Android. It overrides the default UI message pump to allow nested loops.
class NestedMessagePumpAndroid : public base::MessagePumpForUI {
 public:
  NestedMessagePumpAndroid();

  virtual void Run(Delegate* delegate) OVERRIDE;
  virtual void Quit() OVERRIDE;
  virtual void ScheduleWork() OVERRIDE;
  virtual void ScheduleDelayedWork(
      const base::TimeTicks& delayed_work_time) OVERRIDE;
  virtual void Start(Delegate* delegate) OVERRIDE;

  static bool RegisterJni(JNIEnv* env);

 protected:
  virtual ~NestedMessagePumpAndroid();

 private:
  // We may make recursive calls to Run, so we save state that needs to be
  // separate between them in this structure type.
  struct RunState;

  RunState* state_;

  DISALLOW_COPY_AND_ASSIGN(NestedMessagePumpAndroid);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NESTED_MESSAGE_PUMP_ANDROID_
