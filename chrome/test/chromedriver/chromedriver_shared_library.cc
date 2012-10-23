// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <string>

#include "base/at_exit.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chromedriver.h"

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

}  // extern "C"

#if defined(OS_WIN)
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, void* reserved) {
  if (reason == DLL_PROCESS_ATTACH)
    g_at_exit = new base::AtExitManager();
  if (reason == DLL_PROCESS_DETACH)
    delete g_at_exit;
  return TRUE;
}
#else
void __attribute__((constructor)) OnLoad(void) {
  g_at_exit = new base::AtExitManager();
}
void __attribute__((destructor)) OnUnload(void) {
  delete g_at_exit;
}
#endif
