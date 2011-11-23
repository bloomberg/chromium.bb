// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/injection_test_dll.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "sandbox/src/sandbox.h"
#include "skia/ext/skia_sandbox_support_win.h"
#include "unicode/timezone.h"

namespace {

// In order to have Theme support, we need to connect to the theme service.
// This needs to be done before we lock down the renderer. Officially this
// can be done with OpenThemeData() but it fails unless you pass a valid
// window at least the first time. Interestingly, the very act of creating a
// window also sets the connection to the theme service.
void EnableThemeSupportForRenderer(bool no_sandbox) {
  HWINSTA current = NULL;
  HWINSTA winsta0 = NULL;

  if (!no_sandbox) {
    current = ::GetProcessWindowStation();
    winsta0 = ::OpenWindowStationW(L"WinSta0", FALSE, GENERIC_READ);
    if (!winsta0 || !::SetProcessWindowStation(winsta0)) {
      // Could not set the alternate window station. There is a possibility
      // that the theme wont be correctly initialized on XP.
      NOTREACHED() << "Unable to switch to WinSt0";
    }
  }

  HWND window = ::CreateWindowExW(0, L"Static", L"", WS_POPUP | WS_DISABLED,
                                  CW_USEDEFAULT, 0, 0, 0,  HWND_MESSAGE, NULL,
                                  ::GetModuleHandleA(NULL), NULL);
  if (!window) {
    DLOG(WARNING) << "failed to enable theme support";
  } else {
    ::DestroyWindow(window);
  }

  if (!no_sandbox) {
    // Revert the window station.
    if (!current || !::SetProcessWindowStation(current)) {
      // We failed to switch back to the secure window station. This might
      // confuse the renderer enough that we should kill it now.
      LOG(FATAL) << "Failed to restore alternate window station";
    }

    if (!::CloseWindowStation(winsta0)) {
      // We might be leaking a winsta0 handle.  This is a security risk, but
      // since we allow fail over to no desktop protection in low memory
      // condition, this is not a big risk.
      NOTREACHED();
    }
  }
}

// Windows-only skia sandbox support
void SkiaPreCacheFont(LOGFONT logfont) {
  content::RenderThread* render_thread = content::RenderThread::Get();
  if (render_thread) {
    render_thread->PreCacheFont(logfont);
  }
}

}  // namespace

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const content::MainFunctionParams& parameters)
        : parameters_(parameters),
          sandbox_test_module_(NULL) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
  // Be mindful of what resources you acquire here. They can be used by
  // malicious code if the renderer gets compromised.
  const CommandLine& command_line = parameters_.command_line;
  bool no_sandbox = command_line.HasSwitch(switches::kNoSandbox);
  EnableThemeSupportForRenderer(no_sandbox);

  if (!no_sandbox) {
    // ICU DateFormat class (used in base/time_format.cc) needs to get the
    // Olson timezone ID by accessing the registry keys under
    // HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones.
    // After TimeZone::createDefault is called once here, the timezone ID is
    // cached and there's no more need to access the registry. If the sandbox
    // is disabled, we don't have to make this dummy call.
    scoped_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
    SetSkiaEnsureTypefaceAccessible(SkiaPreCacheFont);
  }
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  const CommandLine& command_line = parameters_.command_line;

  DVLOG(1) << "Started renderer with " << command_line.GetCommandLineString();

  sandbox::TargetServices* target_services =
      parameters_.sandbox_info->target_services;

  if (target_services && !no_sandbox) {
      std::wstring test_dll_name =
          command_line.GetSwitchValueNative(switches::kTestSandbox);
    if (!test_dll_name.empty()) {
      sandbox_test_module_ = LoadLibrary(test_dll_name.c_str());
      DCHECK(sandbox_test_module_);
      if (!sandbox_test_module_) {
        return false;
      }
    }
  }
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  sandbox::TargetServices* target_services =
      parameters_.sandbox_info->target_services;

  if (target_services) {
    // Cause advapi32 to load before the sandbox is turned on.
    unsigned int dummy_rand;
    rand_s(&dummy_rand);
    // Warm up language subsystems before the sandbox is turned on.
    ::GetUserDefaultLangID();
    ::GetUserDefaultLCID();

    target_services->LowerToken();
    return true;
  }
  return false;
}

void RendererMainPlatformDelegate::RunSandboxTests() {
  if (sandbox_test_module_) {
    RunRendererTests run_security_tests =
        reinterpret_cast<RunRendererTests>(GetProcAddress(sandbox_test_module_,
                                                          kRenderTestCall));
    DCHECK(run_security_tests);
    if (run_security_tests) {
      int test_count = 0;
      DVLOG(1) << "Running renderer security tests";
      BOOL result = run_security_tests(&test_count);
      CHECK(result) << "Test number " << test_count << " has failed.";
    }
  }
}
