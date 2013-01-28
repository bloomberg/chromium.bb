// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class sets up the environment for running the content browser tests
// inside an android application.

#include <android/log.h>

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "content/public/app/android_library_loader_hooks.h"
#include "content/shell/android/shell_jni_registrar.h"
#include "jni/ContentBrowserTestsActivity_jni.h"

// The main function of the program to be wrapped as an apk.
extern int main(int argc, char** argv);

namespace {

void ParseArgsFromString(const std::string& command_line,
                         std::vector<std::string>* args) {
  StringTokenizer tokenizer(command_line, kWhitespaceASCII);
  tokenizer.set_quote_chars("\"");
  while (tokenizer.GetNext()) {
    std::string token;
    RemoveChars(tokenizer.token(), "\"", &token);
    args->push_back(token);
  }
}

void ParseArgsFromCommandLineFile(std::vector<std::string>* args) {
  // The test runner script writes the command line file in
  // "/data/local/tmp".
  static const char kCommandLineFilePath[] =
      "/data/local/tmp/content-browser-tests-command-line";
  FilePath command_line(kCommandLineFilePath);
  std::string command_line_string;
  if (file_util::ReadFileToString(command_line, &command_line_string)) {
    ParseArgsFromString(command_line_string, args);
  }
}

int ArgsToArgv(const std::vector<std::string>& args,
                std::vector<char*>* argv) {
  // We need to pass in a non-const char**.
  int argc = args.size();

  argv->resize(argc + 1);
  for (int i = 0; i < argc; ++i)
    (*argv)[i] = const_cast<char*>(args[i].c_str());
  (*argv)[argc] = NULL;  // argv must be NULL terminated.

  return argc;
}

class ScopedMainEntryLogger {
 public:
  ScopedMainEntryLogger() {
    printf(">>ScopedMainEntryLogger\n");
  }

  ~ScopedMainEntryLogger() {
    printf("<<ScopedMainEntryLogger\n");
    fflush(stdout);
    fflush(stderr);
  }
};

}  // namespace

static void RunTests(JNIEnv* env,
                     jobject obj,
                     jstring jfiles_dir,
                     jobject app_context) {

  // Command line basic initialization, will be fully initialized later.
  static const char* const kInitialArgv[] = { "ContentBrowserTestsActivity" };
  CommandLine::Init(arraysize(kInitialArgv), kInitialArgv);

  // Set the application context in base.
  base::android::ScopedJavaLocalRef<jobject> scoped_context(
      env, env->NewLocalRef(app_context));
  base::android::InitApplicationContext(scoped_context);
  base::android::RegisterJni(env);

  std::vector<std::string> args;
  ParseArgsFromCommandLineFile(&args);

  // We need to pass in a non-const char**.
  std::vector<char*> argv;
  int argc = ArgsToArgv(args, &argv);

  // Fully initialize command line with arguments.
  CommandLine::ForCurrentProcess()->AppendArguments(
      CommandLine(argc, &argv[0]), false);

  ScopedMainEntryLogger scoped_main_entry_logger;
  main(argc, &argv[0]);
}

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!content::RegisterLibraryLoaderEntryHook(env))
    return -1;

  if (!content::android::RegisterShellJni(env))
    return -1;

  if (!RegisterNativesImpl(env))
    return -1;

  return JNI_VERSION_1_4;
}
