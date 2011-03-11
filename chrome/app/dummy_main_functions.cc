// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/result_codes.h"
#include "content/common/main_function_params.h"

// Native Client binary for 64-bit Windows can run only the NaCl loader or
// the sandbox broker processes. Other process types are not supported.
int BrowserMain(const MainFunctionParams& parameters) {
  return ResultCodes::BAD_PROCESS_TYPE;
}

int RendererMain(const MainFunctionParams& parameters) {
  return ResultCodes::BAD_PROCESS_TYPE;
}

int PluginMain(const MainFunctionParams& parameters) {
  return ResultCodes::BAD_PROCESS_TYPE;
}

int PpapiPluginMain(const MainFunctionParams& parameters) {
  return ResultCodes::BAD_PROCESS_TYPE;
}

int WorkerMain(const MainFunctionParams& parameters) {
  return ResultCodes::BAD_PROCESS_TYPE;
}

int UtilityMain(const MainFunctionParams& parameters) {
  return ResultCodes::BAD_PROCESS_TYPE;
}

int ProfileImportMain(const MainFunctionParams& parameters) {
  return ResultCodes::BAD_PROCESS_TYPE;
}

int ZygoteMain(const MainFunctionParams& parameters) {
  return ResultCodes::BAD_PROCESS_TYPE;
}

int DiagnosticsMain(const CommandLine& command_line) {
  return 1;
}

int GpuMain(const MainFunctionParams&) {
  return ResultCodes::BAD_PROCESS_TYPE;
}

int ServiceProcessMain(const MainFunctionParams& parameters) {
  return ResultCodes::BAD_PROCESS_TYPE;
}
