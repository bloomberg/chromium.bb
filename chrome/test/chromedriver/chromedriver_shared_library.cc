// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <string>

#include "base/at_exit.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chromedriver.h"
#include "chrome/test/chromedriver/command_executor.h"
#include "chrome/test/chromedriver/command_executor_impl.h"
#include "chrome/test/chromedriver/third_party/jni/jni.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_WIN)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

namespace {

base::AtExitManager* g_at_exit = NULL;

}  // namespace

extern "C" {

// Synchronously executes the given command. Thread safe.
// Every call dynamically allocates a |response| string, which must be
// deallocated by calling |Free|. This |response| string is not guaranteed
// to be null terminated, and may contain null characters within. You
// should use the given response size to construct the string.
void EXPORT ExecuteCommand(
    const char* command,
    unsigned int command_size,
    char** response,
    unsigned int* response_size) {
  std::string command_str(command, command_size);
  std::string json;
  ExecuteCommand(command_str, &json);
  *response = new char[json.length()];
  std::memcpy(*response, json.c_str(), json.length());
  *response_size = json.length();
}

void EXPORT Free(char* p) {
  delete [] p;
}

// Temporary measure for running Java WebDriver acceptance tests before we
// add a client for the new ChromeDriver there.
// TODO(kkania): Rename this to
// Java_org_chromium_chromedriver_CommandExecutor_execute.
EXPORT jstring Java_org_openqa_selenium_chrome_NewCommandExecutor_execute(
    JNIEnv *env,
    jclass clazz,
    jstring command) {
  const jchar* cmd_jchars = env->GetStringChars(command, NULL);
  jsize cmd_length = env->GetStringLength(command);
  string16 cmd_utf16(reinterpret_cast<const char16*>(cmd_jchars), cmd_length);
  env->ReleaseStringChars(command, cmd_jchars);

  std::string response_utf8;
  ExecuteCommand(UTF16ToUTF8(cmd_utf16), &response_utf8);

  string16 response_utf16(UTF8ToUTF16(response_utf8));
  return env->NewString(reinterpret_cast<const jchar*>(response_utf16.c_str()),
                        response_utf16.length());
}

}  // extern "C"

#if defined(OS_WIN)
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, void* reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_at_exit = new base::AtExitManager();
    Init(scoped_ptr<CommandExecutor>(new CommandExecutorImpl()));
  }
  if (reason == DLL_PROCESS_DETACH) {
    Shutdown();
    delete g_at_exit;
  }
  return TRUE;
}
#else
void __attribute__((constructor)) OnLoad(void) {
  g_at_exit = new base::AtExitManager();
  Init(scoped_ptr<CommandExecutor>(new CommandExecutorImpl()));
}
void __attribute__((destructor)) OnUnload(void) {
  Shutdown();
  delete g_at_exit;
}
#endif
