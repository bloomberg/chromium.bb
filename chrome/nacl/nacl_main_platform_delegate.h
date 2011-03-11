// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_MAIN_PLATFORM_DELEGATE_H_
#define CHROME_NACL_NACL_MAIN_PLATFORM_DELEGATE_H_
#pragma once

#include "base/native_library.h"
#include "content/common/main_function_params.h"

typedef bool (*RunNaClLoaderTests)(void);
const char kNaClLoaderTestCall[] = "RunNaClLoaderTests";

class NaClMainPlatformDelegate {
 public:
  explicit NaClMainPlatformDelegate(const MainFunctionParams& parameters);
  ~NaClMainPlatformDelegate();

  // Called first thing and last thing in the process' lifecycle, i.e. before
  // the sandbox is enabled.
  void PlatformInitialize();
  void PlatformUninitialize();

  // Gives us an opportunity to initialize state used for tests before enabling
  // the sandbox.
  void InitSandboxTests(bool no_sandbox);

  // Initiate Lockdown.
  void EnableSandbox();

  // Runs the sandbox tests for the NaCl Loader, if tests supplied.
  // Cannot run again, after this (resources freed).
  // Returns false if the tests are supplied and fail.
  bool RunSandboxTests();

 private:
  const MainFunctionParams& parameters_;
  base::NativeLibrary sandbox_test_module_;

  DISALLOW_COPY_AND_ASSIGN(NaClMainPlatformDelegate);
};

#endif  // CHROME_NACL_NACL_MAIN_PLATFORM_DELEGATE_H_
