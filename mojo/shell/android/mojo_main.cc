// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/android/mojo_main.h"

#include "base/android/jni_string.h"
#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "jni/MojoMain_jni.h"
#include "mojo/shell/run.h"
#include "ui/gl/gl_surface_egl.h"

using base::LazyInstance;

namespace mojo {

namespace {

base::AtExitManager* g_at_exit = 0;

LazyInstance<scoped_ptr<base::MessageLoop> > g_java_message_loop =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<scoped_ptr<base::Thread> > g_shell_thread =
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

struct ShellInit {
  scoped_refptr<base::SingleThreadTaskRunner> java_runner;
  base::android::ScopedJavaGlobalRef<jobject> activity;
};

void StartOnShellThread(ShellInit* init) {
  shell::Context* context = new shell::Context();

  context->set_activity(init->activity.obj());
  context->task_runners()->set_java_runner(init->java_runner.get());
  delete init;

  g_context.Get().reset(context);
  shell::Run(context);
}

}  // namspace

static void Init(JNIEnv* env, jclass clazz, jobject context) {
  base::android::ScopedJavaLocalRef<jobject> scoped_context(env, context);

  base::android::InitApplicationContext(scoped_context);

  if (g_at_exit)
    return;
  g_at_exit = new base::AtExitManager();
  // TODO(abarth): Currently we leak g_at_exit.

  CommandLine::Init(0, 0);
  InitializeLogging();

  g_java_message_loop.Get().reset(
      new base::MessageLoop(base::MessageLoop::TYPE_UI));
  base::MessageLoopForUI::current()->Start();

  // TODO(abarth): At which point should we switch to cross-platform
  // initialization?

  gfx::GLSurface::InitializeOneOff();
}

static void Start(JNIEnv* env, jclass clazz, jobject context, jstring jurl) {
  if (jurl) {
    std::string app_url = base::android::ConvertJavaStringToUTF8(env, jurl);
    std::vector<std::string> argv;
    argv.push_back("mojo_shell");
    argv.push_back("--app=" + app_url);
    CommandLine::ForCurrentProcess()->InitFromArgv(argv);
  }

  ShellInit* init = new ShellInit();
  init->java_runner = base::MessageLoopForUI::current()->message_loop_proxy();
  init->activity.Reset(env, context);

  g_shell_thread.Get().reset(new base::Thread("shell_thread"));
  g_shell_thread.Get()->Start();
  g_shell_thread.Get()->message_loop()->PostTask(FROM_HERE,
      base::Bind(StartOnShellThread, init));

  // TODO(abarth): Currently we leak g_shell_thread.
}

bool RegisterMojoMain(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace mojo
