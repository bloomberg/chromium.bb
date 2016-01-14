// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_
#define CONTENT_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/main_function_params.h"

#ifdef __OBJC__
@class NSBundle;
#else
class NSBundle;
#endif  // __OBJC__

namespace content {

class CONTENT_EXPORT RendererMainPlatformDelegate {
 public:
  explicit RendererMainPlatformDelegate(
      const MainFunctionParams& parameters);
  ~RendererMainPlatformDelegate();

  // Called first thing and last thing in the process' lifecycle, i.e. before
  // the sandbox is enabled.
  void PlatformInitialize();
  void PlatformUninitialize();

  // Initiate Lockdown, returns true on success.
  bool EnableSandbox();

 private:
#if defined(OS_WIN)
  const MainFunctionParams& parameters_;
  HMODULE sandbox_test_module_;
#elif defined(OS_MACOSX)
  NSBundle* sandbox_tests_bundle_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RendererMainPlatformDelegate);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_
