// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/crash_reporter/aw_microdump_crash_reporter.h"

#include "android_webview/common/aw_version_info_values.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/scoped_native_library.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/content/app/crash_reporter_client.h"

namespace android_webview {
namespace crash_reporter {

namespace {

class AwCrashReporterClient : public ::crash_reporter::CrashReporterClient {
 public:
  AwCrashReporterClient() : dump_fd_(-1) {}

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

  int GetAndroidMinidumpDescriptor() override { return dump_fd_; }

  bool DumpWithoutCrashingToFd(int fd) {
    base::AutoLock lock(dump_lock_);
    dump_fd_ = fd;
    base::debug::DumpWithoutCrashing();
    dump_fd_ = -1;
    return true;
  }

 private:
  int dump_fd_;
  base::Lock dump_lock_;
  DISALLOW_COPY_AND_ASSIGN(AwCrashReporterClient);
};

base::LazyInstance<AwCrashReporterClient>::Leaky g_crash_reporter_client =
    LAZY_INSTANCE_INITIALIZER;

bool g_enabled = false;

#if defined(ARCH_CPU_X86_FAMILY)
bool SafeToUseSignalHandler() {
  // On X86/64 there are binary translators that handle SIGSEGV in userspace and
  // may get chained after our handler - see http://crbug.com/477444
  // We attempt to detect this to work out when it's safe to install breakpad.
  // If anything doesn't seem right we assume it's not safe.

  // type and mangled name of android::NativeBridgeInitialized
  typedef bool (*InitializedFunc)();
  const char kInitializedSymbol[] = "_ZN7android23NativeBridgeInitializedEv";
  // type and mangled name of android::NativeBridgeGetVersion
  typedef uint32_t (*VersionFunc)();
  const char kVersionSymbol[] = "_ZN7android22NativeBridgeGetVersionEv";

  base::ScopedNativeLibrary lib_native_bridge(
      base::FilePath("libnativebridge.so"));
  if (!lib_native_bridge.is_valid()) {
    DLOG(WARNING) << "Couldn't load libnativebridge";
    return false;
  }

  InitializedFunc NativeBridgeInitialized = reinterpret_cast<InitializedFunc>(
      lib_native_bridge.GetFunctionPointer(kInitializedSymbol));
  if (NativeBridgeInitialized == nullptr) {
    DLOG(WARNING) << "Couldn't tell if native bridge initialized";
    return false;
  }
  if (!NativeBridgeInitialized()) {
    // Native process, safe to use breakpad.
    return true;
  }

  VersionFunc NativeBridgeGetVersion = reinterpret_cast<VersionFunc>(
      lib_native_bridge.GetFunctionPointer(kVersionSymbol));
  if (NativeBridgeGetVersion == nullptr) {
    DLOG(WARNING) << "Couldn't get native bridge version";
    return false;
  }
  uint32_t version = NativeBridgeGetVersion();
  if (version >= 2) {
    // Native bridge at least version 2, safe to use breakpad.
    return true;
  } else {
    DLOG(WARNING) << "Native bridge ver=" << version << "; too low";
    return false;
  }
}
#endif

}  // namespace

void EnableMicrodumpCrashReporter() {
  if (g_enabled) {
    NOTREACHED() << "EnableMicrodumpCrashReporter called more than once";
    return;
  }

#if defined(ARCH_CPU_X86_FAMILY)
  if (!SafeToUseSignalHandler()) {
    LOG(WARNING) << "Can't use breakpad to handle WebView crashes";
    return;
  }
#endif

  ::crash_reporter::SetCrashReporterClient(g_crash_reporter_client.Pointer());

  // |process_type| is not really relevant here, as long as it not empty.
  breakpad::InitNonBrowserCrashReporterForAndroid("webview" /* process_type */);
  g_enabled = true;
}

void AddGpuFingerprintToMicrodumpCrashHandler(
    const std::string& gpu_fingerprint) {
  breakpad::AddGpuFingerprintToMicrodumpCrashHandler(gpu_fingerprint);
}

bool DumpWithoutCrashingToFd(int fd) {
  return g_crash_reporter_client.Pointer()->DumpWithoutCrashingToFd(fd);
}

}  // namespace crash_reporter
}  // namespace android_webview
