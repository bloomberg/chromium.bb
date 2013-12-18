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
#include "base/message_loop/message_loop.h"
#include "jni/MojoMain_jni.h"
#include "mojo/services/native_viewport/native_viewport_service.h"
#include "mojo/shell/context.h"
#include "mojo/shell/init.h"
#include "mojo/shell/run.h"
#include "mojo/shell/service_manager.h"
#include "ui/gl/gl_surface_egl.h"

using base::LazyInstance;

namespace mojo {

namespace {

base::AtExitManager* g_at_exit = 0;

LazyInstance<scoped_ptr<base::MessageLoop> > g_java_message_loop =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<scoped_ptr<shell::Context> > g_context =
    LAZY_INSTANCE_INITIALIZER;

class NativeViewportServiceLoader : public shell::ServiceManager::Loader {
 public:
  NativeViewportServiceLoader() {}
  virtual ~NativeViewportServiceLoader() {}

 private:
  virtual void Load(const GURL& url,
                    shell::ServiceManager* manager,
                    ScopedMessagePipeHandle service_handle)
      MOJO_OVERRIDE {
    service_.reset(CreateNativeViewportService(service_handle.Pass()));
    service_->set_context(g_context.Get().get());
  }
  scoped_ptr<services::NativeViewportService> service_;
};

LazyInstance<scoped_ptr<NativeViewportServiceLoader> >
    g_viewport_service_loader = LAZY_INSTANCE_INITIALIZER;

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
    argv.push_back(app_url);
    CommandLine::ForCurrentProcess()->InitFromArgv(argv);
  }

  base::android::ScopedJavaGlobalRef<jobject> activity;
  activity.Reset(env, context);

  shell::Context* shell_context = new shell::Context();
  shell_context->set_activity(activity.obj());
  g_viewport_service_loader.Get().reset(new NativeViewportServiceLoader());
  shell_context->service_manager()->SetLoaderForURL(
    g_viewport_service_loader.Get().get(),
    GURL("mojo:mojo_native_viewport_service"));

  g_context.Get().reset(shell_context);
  shell::Run(shell_context);
}

bool RegisterMojoMain(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace mojo
