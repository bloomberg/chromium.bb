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
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/simple_thread.h"
#include "jni/ShellMain_jni.h"
#include "mojo/common/message_pump_mojo.h"
#include "mojo/runner/android/android_handler_loader.h"
#include "mojo/runner/android/background_application_loader.h"
#include "mojo/runner/android/native_viewport_application_loader.h"
#include "mojo/runner/android/ui_application_loader_android.h"
#include "mojo/runner/context.h"
#include "mojo/runner/init.h"
#include "mojo/shell/application_loader.h"
#include "ui/gl/gl_surface_egl.h"

using base::LazyInstance;

namespace mojo {
namespace runner {

namespace {

// Tag for logging.
const char kLogTag[] = "chromium";

// Command line argument for the communication fifo.
const char kFifoPath[] = "fifo-path";

class MojoShellRunner : public base::DelegateSimpleThread::Delegate {
 public:
  MojoShellRunner(const std::vector<std::string>& parameters) {}
  ~MojoShellRunner() override {}

 private:
  void Run() override;

  DISALLOW_COPY_AND_ASSIGN(MojoShellRunner);
};

LazyInstance<scoped_ptr<base::MessageLoop>> g_java_message_loop =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<scoped_ptr<Context>> g_context = LAZY_INSTANCE_INITIALIZER;

LazyInstance<scoped_ptr<MojoShellRunner>> g_shell_runner =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<scoped_ptr<base::DelegateSimpleThread>> g_shell_thread =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<base::android::ScopedJavaGlobalRef<jobject>> g_main_activiy =
    LAZY_INSTANCE_INITIALIZER;

void ConfigureAndroidServices(Context* context) {
  context->application_manager()->SetLoaderForURL(
      make_scoped_ptr(new UIApplicationLoader(
          make_scoped_ptr(new NativeViewportApplicationLoader()),
          g_java_message_loop.Get().get())),
      GURL("mojo:native_viewport_service"));

  // Android handler is bundled with the Mojo shell, because it uses the
  // MojoShell application as the JNI bridge to bootstrap execution of other
  // Android Mojo apps that need JNI.
  context->application_manager()->SetLoaderForURL(
      make_scoped_ptr(new BackgroundApplicationLoader(
          make_scoped_ptr(new AndroidHandlerLoader()), "android_handler",
          base::MessageLoop::TYPE_DEFAULT)),
      GURL("mojo:android_handler"));
}

void QuitShellThread() {
  g_shell_thread.Get()->Join();
  g_shell_thread.Pointer()->reset();
  Java_ShellMain_finishActivity(base::android::AttachCurrentThread(),
                                g_main_activiy.Get().obj());
  exit(0);
}

void MojoShellRunner::Run() {
  base::MessageLoop loop(common::MessagePumpMojo::Create());
  Context* context = g_context.Pointer()->get();
  ConfigureAndroidServices(context);
  context->Init();

  context->Run(GURL("mojo:window_manager"));
  loop.Run();

  g_java_message_loop.Pointer()->get()->PostTask(FROM_HERE,
                                                 base::Bind(&QuitShellThread));
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
                 jclass clazz,
                 jobject activity,
                 jstring mojo_shell_path,
                 jobjectArray jparameters,
                 jstring j_local_apps_directory,
                 jstring j_tmp_dir) {
  g_main_activiy.Get().Reset(env, activity);

  // Setting the TMPDIR environment variable so that applications can use it.
  // TODO(qsr) We will need our subprocesses to inherit this.
  int return_value =
      setenv("TMPDIR",
             base::android::ConvertJavaStringToUTF8(env, j_tmp_dir).c_str(), 1);
  DCHECK_EQ(return_value, 0);

  base::android::ScopedJavaLocalRef<jobject> scoped_activity(env, activity);
  base::android::InitApplicationContext(env, scoped_activity);

  std::vector<std::string> parameters;
  parameters.push_back(
      base::android::ConvertJavaStringToUTF8(env, mojo_shell_path));
  base::android::AppendJavaStringArrayToStringVector(env, jparameters,
                                                     &parameters);
  base::CommandLine::Init(0, nullptr);
  base::CommandLine::ForCurrentProcess()->InitFromArgv(parameters);
  g_shell_runner.Get().reset(new MojoShellRunner(parameters));

  InitializeLogging();
  mojo::runner::WaitForDebuggerIfNecessary();

  InitializeRedirection();

  // We want ~MessageLoop to happen prior to ~Context. Initializing
  // LazyInstances is akin to stack-allocating objects; their destructors
  // will be invoked first-in-last-out.
  Context* shell_context = new Context();
  shell_context->SetShellFileRoot(base::FilePath(
      base::android::ConvertJavaStringToUTF8(env, j_local_apps_directory)));
  g_context.Get().reset(shell_context);

  g_java_message_loop.Get().reset(new base::MessageLoopForUI);
  base::MessageLoopForUI::current()->Start();

  // TODO(abarth): At which point should we switch to cross-platform
  // initialization?

  gfx::GLSurface::InitializeOneOff();
}

static jboolean Start(JNIEnv* env, jclass clazz) {
  if (!base::CommandLine::ForCurrentProcess()->GetArgs().size())
    return false;

#if defined(MOJO_SHELL_DEBUG_URL)
  base::CommandLine::ForCurrentProcess()->AppendArg(MOJO_SHELL_DEBUG_URL);
  // Sleep for 5 seconds to give the debugger a chance to attach.
  sleep(5);
#endif

  g_shell_thread.Get().reset(new base::DelegateSimpleThread(
      g_shell_runner.Get().get(), "ShellThread"));
  g_shell_thread.Get()->Start();
  return true;
}

static void AddApplicationURL(JNIEnv* env, jclass clazz, jstring jurl) {
  base::CommandLine::ForCurrentProcess()->AppendArg(
      base::android::ConvertJavaStringToUTF8(env, jurl));
}

bool RegisterShellMain(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace runner
}  // namespace mojo

// TODO(vtl): Even though main() should never be called, mojo_shell fails to
// link without it. Figure out if we can avoid this.
int main(int argc, char** argv) {
  NOTREACHED();
}
