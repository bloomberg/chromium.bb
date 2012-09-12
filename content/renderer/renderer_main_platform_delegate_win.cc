// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include <signal.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/win_util.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/injection_test_win.h"
#include "content/public/renderer/render_thread.h"
#include "sandbox/win/src/sandbox.h"
#include "skia/ext/skia_sandbox_support_win.h"
#include "unicode/timezone.h"

namespace {

// Windows-only skia sandbox support
void SkiaPreCacheFont(const LOGFONT& logfont) {
  content::RenderThread* render_thread = content::RenderThread::Get();
  if (render_thread) {
    render_thread->PreCacheFont(logfont);
  }
}

void __cdecl ForceCrashOnSigAbort(int) {
  *((int*)0) = 0x1337;
}

void InitExitInterceptions() {
  // If code subsequently tries to exit using exit(), _exit(), abort(), or
  // ExitProcess(), force a crash (since otherwise these would be silent
  // terminations and fly under the radar).
  base::win::SetShouldCrashOnProcessDetach(true);

  // Prevent CRT's abort code from prompting a dialog or trying to "report" it.
  // Disabling the _CALL_REPORTFAULT behavior is important since otherwise it
  // has the sideffect of clearing our exception filter, which means we
  // don't get any crash.
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

  // Set a SIGABRT handler for good measure. We will crash even if the default
  // is left in place, however this allows us to crash earlier. And it also
  // lets us crash in response to code which might directly call raise(SIGABRT)
  signal(SIGABRT, ForceCrashOnSigAbort);
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
  InitExitInterceptions();

  // Be mindful of what resources you acquire here. They can be used by
  // malicious code if the renderer gets compromised.
  const CommandLine& command_line = parameters_.command_line;
  bool no_sandbox = command_line.HasSwitch(switches::kNoSandbox);

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
  // At this point we are shutting down in a normal code path, so undo our
  // hack to crash on exit.
  base::win::SetShouldCrashOnProcessDetach(false);
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
