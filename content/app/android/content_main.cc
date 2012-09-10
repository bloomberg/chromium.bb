// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/content_main.h"

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/common/content_switches.h"
#include "jni/ContentMain_jni.h"

using base::LazyInstance;
using content::ContentMainRunner;
using content::ContentMainDelegate;

namespace {
LazyInstance<scoped_ptr<ContentMainRunner> > g_content_runner =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<scoped_ptr<ContentMainDelegate> > g_content_main_delegate =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace content {

static void InitApplicationContext(JNIEnv* env, jclass clazz, jobject context) {
  base::android::ScopedJavaLocalRef<jobject> scoped_context(env, context);
  base::android::InitApplicationContext(scoped_context);
}

static jint Start(JNIEnv* env, jclass clazz) {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();

  // This is only for browser process. We want to start waiting as early as
  // possible though here is the common initialization code.
  if (parsed_command_line.HasSwitch(switches::kWaitForDebugger) &&
      "" == parsed_command_line.GetSwitchValueASCII(switches::kProcessType)) {
    LOG(ERROR) << "Browser waiting for GDB because flag "
               << switches::kWaitForDebugger << " was supplied.";
    base::debug::WaitForDebugger(24*60*60, false);
  }

  DCHECK(!g_content_runner.Get().get());
  g_content_runner.Get().reset(ContentMainRunner::Create());
  g_content_runner.Get()->Initialize(0, NULL,
                                     g_content_main_delegate.Get().get());
  return g_content_runner.Get()->Run();
}

void SetContentMainDelegate(ContentMainDelegate* delegate) {
  DCHECK(!g_content_main_delegate.Get().get());
  g_content_main_delegate.Get().reset(delegate);
}

bool RegisterContentMain(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
