// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <string.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_pump_android.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"

namespace {

// The test implementation of AndroidOS stores everything in the following
// directory.
const char* kAndroidTestTempDirectory = "/data/local/tmp";

struct RunState {
  RunState(base::MessagePump::Delegate* delegate, int run_depth)
      : delegate(delegate),
        run_depth(run_depth),
        should_quit(false) {
  }

  base::MessagePump::Delegate* delegate;

  // Used to count how many Run() invocations are on the stack.
  int run_depth;

  // Used to flag that the current Run() invocation should return ASAP.
  bool should_quit;
};

RunState* g_state = NULL;

// A singleton WaitableEvent wrapper so we avoid a busy loop in
// MessagePumpForUIStub. Other platforms use the native event loop which blocks
// when there are no pending messages.
class Waitable {
 public:
   static Waitable* GetInstance() {
     return Singleton<Waitable>::get();
   }

   // Signals that there are more work to do.
   void Signal() {
     waitable_event_.Signal();
   }

   // Blocks until more work is scheduled.
   void Block() {
     waitable_event_.Wait();
   }

   void Quit() {
     g_state->should_quit = true;
     Signal();
   }

 private:
  friend struct DefaultSingletonTraits<Waitable>;

  Waitable()
      : waitable_event_(false, false) {
  }

  base::WaitableEvent waitable_event_;
};

// The MessagePumpForUI implementation for test purpose.
class MessagePumpForUIStub : public base::MessagePumpForUI {
  void Start(base::MessagePump::Delegate* delegate) {
    NOTREACHED() << "The Start() method shouldn't be called in test, using"
        " Run() method should be used.";
  }

  void Run(base::MessagePump::Delegate* delegate) {
    // The following was based on message_pump_glib.cc, except we're using a
    // WaitableEvent since there are no native message loop to use.
    RunState state(delegate, g_state ? g_state->run_depth + 1 : 1);

    RunState* previous_state = g_state;
    g_state = &state;

    bool more_work_is_plausible = true;

    for (;;) {
      if (!more_work_is_plausible) {
        Waitable::GetInstance()->Block();
        if (g_state->should_quit)
          break;
      }

      more_work_is_plausible = g_state->delegate->DoWork();
      if (g_state->should_quit)
        break;

      base::TimeTicks delayed_work_time;
      more_work_is_plausible |=
          g_state->delegate->DoDelayedWork(&delayed_work_time);
      if (g_state->should_quit)
        break;

      if (more_work_is_plausible)
        continue;

      more_work_is_plausible = g_state->delegate->DoIdleWork();
      if (g_state->should_quit)
        break;

      more_work_is_plausible |= !delayed_work_time.is_null();
    }

    g_state = previous_state;
  }

  void Quit() {
    Waitable::GetInstance()->Quit();
  }

  void ScheduleWork() {
    Waitable::GetInstance()->Signal();
  }

  void ScheduleDelayedWork(const base::TimeTicks& delayed_work_time) {
    Waitable::GetInstance()->Signal();
  }
};

// Provides the test path for DIR_MODULE, DIR_CACHE and DIR_ANDROID_APP_DATA.
bool PathTestProviderAndroid(int key, FilePath* result) {
  switch (key) {
    case base::DIR_MODULE: {
      *result = FilePath(kAndroidTestTempDirectory);
      return true;
    }
    case base::DIR_CACHE: {
      *result = FilePath(kAndroidTestTempDirectory);
      return true;
    }
    case base::DIR_ANDROID_APP_DATA: {
      *result = FilePath(kAndroidTestTempDirectory);
      return true;
    }
    default:
      return false;
  }
}

// The factory method to create a MessagePumpForUI.
base::MessagePump* CreateMessagePumpForUIStub() {
  return new MessagePumpForUIStub();
}

}  // namespace

void InitAndroidTestStub() {
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::DELETE_OLD_LOG_FILE,
                       logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  // To view log output with IDs and timestamps use "adb logcat -v threadtime".
  logging::SetLogItems(false,    // Process ID
                       false,    // Thread ID
                       false,    // Timestamp
                       false);   // Tick count

  PathService::RegisterProvider(&PathTestProviderAndroid, base::DIR_MODULE,
                                base::DIR_MODULE + 1);
  PathService::RegisterProvider(&PathTestProviderAndroid, base::DIR_CACHE,
                                base::DIR_CACHE + 1);
  PathService::RegisterProvider(&PathTestProviderAndroid,
      base::DIR_ANDROID_APP_DATA, base::DIR_ANDROID_APP_DATA + 1);

  MessageLoop::InitMessagePumpForUIFactory(&CreateMessagePumpForUIStub);
}

// TODO(michaelbai): The below DetachFromVM was added because we excluded the
// jni_android.{h|cc} which require JNI to compile. Remove them when those 2
// files added.
namespace base {

namespace android {

void DetachFromVM() {}

}  // namespace android

// TODO(michaelbai): The below MessagePumpForUI were added because we excluded
// message_pump_android.{h|cc} which require JNI to compile. Remove them when
// those 2 files added.
MessagePumpForUI::MessagePumpForUI()
    : state_(NULL) {
}

MessagePumpForUI::~MessagePumpForUI() {}

void MessagePumpForUI::Run(Delegate* delegate) {}

void MessagePumpForUI::Start(Delegate* delegate) {}

void MessagePumpForUI::Quit() {}

void MessagePumpForUI::ScheduleWork() {}

void MessagePumpForUI::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
}

// TODO(michaelbai): The below PathProviderAndroid was added because we
// excluded base_paths_android.cc which requires JNI to compile. Remove them
// when this file added.
bool PathProviderAndroid(int key, FilePath* result) {
  return false;
}

}  // namespace base
