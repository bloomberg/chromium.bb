// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/android/mojo_main.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "jni/MojoMain_jni.h"
#include "mojo/shell/run.h"

using base::LazyInstance;

namespace mojo {

namespace {

base::AtExitManager* g_at_exit = 0;

LazyInstance<scoped_ptr<base::Thread> > g_main_thread =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<scoped_ptr<shell::Context> > g_context =
    LAZY_INSTANCE_INITIALIZER;

void InitializeLogging() {
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  settings.dcheck_state =
      logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS;
  logging::InitLogging(settings);
  // To view log output with IDs and timestamps use "adb logcat -v threadtime".
  logging::SetLogItems(false,    // Process ID
                       false,    // Thread ID
                       false,    // Timestamp
                       false);   // Tick count
}

void StartOnShellThread() {
  g_context.Get().reset(new shell::Context());
  shell::Run(g_context.Get().get());
}

}  // namspace

static void Start(JNIEnv* env, jclass clazz, jobject context) {
  base::android::ScopedJavaLocalRef<jobject> scoped_context(env, context);
  base::android::InitApplicationContext(scoped_context);

  if (g_at_exit)
    return;
  g_at_exit = new base::AtExitManager();

  CommandLine::Init(0, 0);
  InitializeLogging();

  g_main_thread.Get().reset(new base::Thread("shell_thread"));
  g_main_thread.Get()->Start();
  g_main_thread.Get()->message_loop()->PostTask(FROM_HERE,
      base::Bind(StartOnShellThread));

  // TODO(abarth): Currently we leak g_at_exit and g_main_thread.
}

bool RegisterMojoMain(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace mojo
