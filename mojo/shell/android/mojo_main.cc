// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/android/mojo_main.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "jni/MojoMain_jni.h"
#include "mojo/shell/run.h"

using base::LazyInstance;

namespace mojo {

namespace {

base::AtExitManager* g_at_exit = 0;

LazyInstance<scoped_ptr<base::Thread> > g_main_thread =
    LAZY_INSTANCE_INITIALIZER;

}  // namspace

static void InitApplicationContext(JNIEnv* env, jclass clazz, jobject context) {
  base::android::ScopedJavaLocalRef<jobject> scoped_context(env, context);
  base::android::InitApplicationContext(scoped_context);

  if (g_at_exit)
    return;

  g_at_exit = new base::AtExitManager();
  CommandLine::Init(0, 0);
  g_main_thread.Get().reset(new base::Thread("shell_thread"));
  g_main_thread.Get()->Start();
  g_main_thread.Get()->message_loop()->PostTask(FROM_HERE,
      base::Bind(shell::Run));
  // TODO(abarth): Currently we leak g_at_exit and g_main_thread.
}

bool RegisterMojoMain(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace mojo
