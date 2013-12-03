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
#include "jni/MojoMain_jni.h"
#include "mojo/shell/init.h"
#include "mojo/shell/run.h"
#include "ui/gl/gl_surface_egl.h"

using base::LazyInstance;

namespace mojo {

namespace {

base::AtExitManager* g_at_exit = 0;

LazyInstance<scoped_ptr<base::MessageLoop> > g_java_message_loop =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<scoped_ptr<shell::Context> > g_context =
    LAZY_INSTANCE_INITIALIZER;

}  // namspace

static void Init(JNIEnv* env, jclass clazz, jobject context) {
  base::android::ScopedJavaLocalRef<jobject> scoped_context(env, context);

  base::android::InitApplicationContext(env, scoped_context);

  if (g_at_exit)
    return;
  g_at_exit = new base::AtExitManager();
  // TODO(abarth): Currently we leak g_at_exit.

  CommandLine::Init(0, 0);
  mojo::shell::InitializeLogging();

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

  base::android::ScopedJavaGlobalRef<jobject> activity;
  activity.Reset(env, context);

  shell::Context* shell_context = new shell::Context();
  shell_context->set_activity(activity.obj());
  g_context.Get().reset(shell_context);
  shell::Run(shell_context);
}

bool RegisterMojoMain(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace mojo
