// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/crash_reporter/aw_microdump_crash_reporter.h"

#include "android_webview/common/aw_version_info_values.h"
#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "components/crash/app/breakpad_linux.h"
#include "components/crash/app/crash_reporter_client.h"

namespace android_webview {
namespace crash_reporter {

namespace {

class AwCrashReporterClient : public ::crash_reporter::CrashReporterClient {
 public:
  AwCrashReporterClient() {}

  // crash_reporter::CrashReporterClient implementation.
  bool IsRunningUnattended() override { return false; }
  bool GetCollectStatsConsent() override { return false; }

  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override {
    *product_name = "WebView";
    *version = PRODUCT_VERSION;
  }
  // Microdumps are always enabled in WebView builds, conversely to what happens
  // in the case of the other Chrome for Android builds (where they are enabled
  // only when NO_UNWIND_TABLES == 1).
  bool ShouldEnableBreakpadMicrodumps() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(AwCrashReporterClient);
};

base::LazyInstance<AwCrashReporterClient>::Leaky g_crash_reporter_client =
    LAZY_INSTANCE_INITIALIZER;

bool g_enabled = false;

}  // namespace

void EnableMicrodumpCrashReporter() {
#if defined(ARCH_CPU_X86_FAMILY)
  // Don't install signal handler on X86/64 because this breaks binary
  // translators that handle SIGSEGV in userspace and get chained after our
  // handler. See crbug.com/477444
  return;
#endif

  if (g_enabled) {
    NOTREACHED() << "EnableMicrodumpCrashReporter called more than once";
    return;
  }

  ::crash_reporter::SetCrashReporterClient(g_crash_reporter_client.Pointer());

  // |process_type| is not really relevant here, as long as it not empty.
  breakpad::InitNonBrowserCrashReporterForAndroid("webview" /* process_type */);
  g_enabled = true;
}

}  // namespace crash_reporter
}  // namespace android_webview
