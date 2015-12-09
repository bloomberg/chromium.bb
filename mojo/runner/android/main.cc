// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/android/main.h"

#include "base/android/fifo_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/mus/android_loader.h"
#include "jni/ShellMain_jni.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/runner/android/android_handler_loader.h"
#include "mojo/runner/android/background_application_loader.h"
#include "mojo/runner/android/context_init.h"
#include "mojo/runner/android/ui_application_loader_android.h"
#include "mojo/runner/context.h"
#include "mojo/runner/host/child_process.h"
#include "mojo/runner/init.h"
#include "mojo/shell/application_loader.h"
#include "ui/gl/gl_surface_egl.h"

using base::LazyInstance;

namespace mojo {
namespace runner {

namespace {

// Command line argument for the communication fifo.
const char kFifoPath[] = "fifo-path";

LazyInstance<scoped_ptr<base::MessageLoop>> g_java_message_loop =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<scoped_ptr<Context>> g_context = LAZY_INSTANCE_INITIALIZER;

LazyInstance<base::android::ScopedJavaGlobalRef<jobject>> g_main_activiy =
    LAZY_INSTANCE_INITIALIZER;

void ConfigureAndroidServices(Context* context) {
  CHECK(context->application_manager());
  context->application_manager()->SetLoaderForURL(
      make_scoped_ptr(
          new UIApplicationLoader(make_scoped_ptr(new mus::AndroidLoader()),
                                  g_java_message_loop.Get().get())),
      GURL("mojo:mus"));

  // Android handler is bundled with the Mojo shell, because it uses the
  // MojoShell application as the JNI bridge to bootstrap execution of other
  // Android Mojo apps that need JNI.
  context->application_manager()->SetLoaderForURL(
      make_scoped_ptr(new BackgroundApplicationLoader(
          make_scoped_ptr(new AndroidHandlerLoader()), "android_handler",
          base::MessageLoop::TYPE_DEFAULT)),
      GURL("mojo:android_handler"));
}

void ExitShell() {
  Java_ShellMain_finishActivity(base::android::AttachCurrentThread(),
                                g_main_activiy.Get().obj());
  exit(0);
}

// Initialize stdout redirection if the command line switch is present.
void InitializeRedirection() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(kFifoPath))
    return;

  base::FilePath fifo_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(kFifoPath);
  base::FilePath directory = fifo_path.DirName();
  CHECK(base::CreateDirectoryAndGetError(directory, nullptr))
      << "Unable to create directory: " << directory.value();
  unlink(fifo_path.value().c_str());
  CHECK(base::android::CreateFIFO(fifo_path, 0666))
      << "Unable to create fifo: " << fifo_path.value();
  CHECK(base::android::RedirectStream(stdout, fifo_path, "w"))
      << "Failed to redirect stdout to file: " << fifo_path.value();
  CHECK(dup2(STDOUT_FILENO, STDERR_FILENO) != -1)
      << "Unable to redirect stderr to stdout.";
}

}  // namespace

static void Init(JNIEnv* env,
                 const JavaParamRef<jclass>& clazz,
                 const JavaParamRef<jobject>& activity,
                 const JavaParamRef<jstring>& mojo_shell_path,
                 const JavaParamRef<jobjectArray>& jparameters,
                 const JavaParamRef<jstring>& j_local_apps_directory,
                 const JavaParamRef<jstring>& j_tmp_dir) {
  g_main_activiy.Get().Reset(env, activity);

  // Setting the TMPDIR environment variable so that applications can use it.
  // TODO(qsr) We will need our subprocesses to inherit this.
  int return_value =
      setenv("TMPDIR",
             base::android::ConvertJavaStringToUTF8(env, j_tmp_dir).c_str(), 1);
  DCHECK_EQ(return_value, 0);

  std::vector<std::string> parameters;
  parameters.push_back(
      base::android::ConvertJavaStringToUTF8(env, mojo_shell_path));
  base::android::AppendJavaStringArrayToStringVector(env, jparameters,
                                                     &parameters);
  base::CommandLine::Init(0, nullptr);
  base::CommandLine::ForCurrentProcess()->InitFromArgv(parameters);

  InitializeLogging();
  mojo::runner::WaitForDebuggerIfNecessary();

  InitializeRedirection();

  // We want ~MessageLoop to happen prior to ~Context. Initializing
  // LazyInstances is akin to stack-allocating objects; their destructors
  // will be invoked first-in-last-out.
  base::FilePath shell_file_root(
      base::android::ConvertJavaStringToUTF8(env, j_local_apps_directory));
  Context* shell_context = new Context;
  g_context.Get().reset(shell_context);

  g_java_message_loop.Get().reset(new base::MessageLoopForUI);
  base::MessageLoopForUI::current()->Start();

  shell_context->Init(shell_file_root);
  ConfigureAndroidServices(shell_context);

  // This is done after the main message loop is started since it may post
  // tasks. This is consistent with the ordering from the desktop version of
  // this file (../desktop/launcher_process.cc).
  InitContext(shell_context);

  // TODO(abarth): At which point should we switch to cross-platform
  // initialization?

  gfx::GLSurface::InitializeOneOff();
}

static void Start(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
#if defined(MOJO_SHELL_DEBUG_URL)
  base::CommandLine::ForCurrentProcess()->AppendArg(MOJO_SHELL_DEBUG_URL);
  // Sleep for 5 seconds to give the debugger a chance to attach.
  sleep(5);
#endif

  Context* context = g_context.Pointer()->get();
  context->RunCommandLineApplication(base::Bind(ExitShell));
}

static void AddApplicationURL(JNIEnv* env,
                              const JavaParamRef<jclass>& clazz,
                              const JavaParamRef<jstring>& jurl) {
  base::CommandLine::ForCurrentProcess()->AppendArg(
      base::android::ConvertJavaStringToUTF8(env, jurl));
}

bool RegisterShellMain(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

Context* GetContext() {
  return g_context.Get().get();
}

}  // namespace runner
}  // namespace mojo

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::runner::InitializeLogging();
  return mojo::runner::ChildProcessMain();
}
