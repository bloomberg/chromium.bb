// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "content/public/common/content_switches.h"

#if defined(OS_CHROMEOS)
#include "chrome/common/chrome_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_switches.h"
#endif

namespace crash_keys {

const char kActiveURL[] = "url-chunk";

const char kExtensionID[] = "extension-%" PRIuS;
const char kNumExtensionsCount[] = "num-extensions";

const char kShutdownType[] = "shutdown-type";
const char kBrowserUnpinTrace[] = "browser-unpin-trace";

const char kPrinterInfo[] = "prn-info-%" PRIuS;

const char kViewCount[] = "view-count";

size_t RegisterChromeCrashKeys() {
  // The following keys may be chunked by the underlying crash logging system,
  // but ultimately constitute a single key-value pair.
  //
  // If you're adding keys here, please also add them to the following lists:
  // chrome/app/chrome_crash_reporter_client_win.cc::RegisterCrashKeysHelper(),
  // android_webview/common/crash_reporter/crash_keys.cc::
  //     RegisterWebViewCrashKeys(),
  // chromecast/crash/cast_crash_keys.cc::RegisterCastCrashKeys().
  base::debug::CrashKey fixed_keys[] = {
#if defined(OS_MACOSX) || defined(OS_WIN)
    {kMetricsClientId, kSmallSize},
#else
    {kClientId, kSmallSize},
#endif
    {kChannel, kSmallSize},
    {kActiveURL, kLargeSize},
    {kNumVariations, kSmallSize},
    {kVariations, kHugeSize},
    {kNumExtensionsCount, kSmallSize},
    {kShutdownType, kSmallSize},
    {kBrowserUnpinTrace, kMediumSize},

    {kViewCount, kSmallSize},

    // TODO(sunnyps): Remove after fixing crbug.com/724999.
    {"gl-context-set-current-stack-trace", kMediumSize},
  };

  // This dynamic set of keys is used for sets of key value pairs when gathering
  // a collection of data, like command line switches or extension IDs.
  std::vector<base::debug::CrashKey> keys(
      fixed_keys, fixed_keys + arraysize(fixed_keys));

  crash_keys::GetCrashKeysForCommandLineSwitches(&keys);

  // Register the extension IDs.
  {
    static char formatted_keys[kExtensionIDMaxCount][sizeof(kExtensionID) + 1] =
        {{ 0 }};
    const size_t formatted_key_len = sizeof(formatted_keys[0]);
    for (size_t i = 0; i < kExtensionIDMaxCount; ++i) {
      int n = base::snprintf(
          formatted_keys[i], formatted_key_len, kExtensionID, i + 1);
      DCHECK_GT(n, 0);
      base::debug::CrashKey crash_key = { formatted_keys[i], kSmallSize };
      keys.push_back(crash_key);
    }
  }

  // Register the printer info.
  {
    static char formatted_keys[kPrinterInfoCount][sizeof(kPrinterInfo) + 1] =
        {{ 0 }};
    const size_t formatted_key_len = sizeof(formatted_keys[0]);
    for (size_t i = 0; i < kPrinterInfoCount; ++i) {
      // Key names are 1-indexed.
      int n = base::snprintf(
          formatted_keys[i], formatted_key_len, kPrinterInfo, i + 1);
      DCHECK_GT(n, 0);
      base::debug::CrashKey crash_key = { formatted_keys[i], kSmallSize };
      keys.push_back(crash_key);
    }
  }

  return base::debug::InitCrashKeys(&keys.at(0), keys.size(), kChunkMaxLength);
}

static bool IsBoringSwitch(const std::string& flag) {
  static const char* const kIgnoreSwitches[] = {
    switches::kEnableLogging,
    switches::kFlagSwitchesBegin,
    switches::kFlagSwitchesEnd,
    switches::kLoggingLevel,
    switches::kProcessType,
    switches::kV,
    switches::kVModule,
#if defined(OS_WIN)
    switches::kForceFieldTrials,
#elif defined(OS_MACOSX)
    switches::kMetricsClientID,
#elif defined(OS_CHROMEOS)
    switches::kPpapiFlashArgs,
    switches::kPpapiFlashPath,
    switches::kRegisterPepperPlugins,
    switches::kUseGL,
    switches::kUserDataDir,
    // Cros/CC flags are specified as raw strings to avoid dependency.
    "child-wallpaper-large",
    "child-wallpaper-small",
    "default-wallpaper-large",
    "default-wallpaper-small",
    "guest-wallpaper-large",
    "guest-wallpaper-small",
    "enterprise-enable-forced-re-enrollment",
    "enterprise-enrollment-initial-modulus",
    "enterprise-enrollment-modulus-limit",
    "login-profile",
    "login-user",
    "max-unused-resource-memory-usage-percentage",
    "termination-message-file",
    "use-cras",
#endif
  };

#if defined(OS_WIN)
  // Just about everything has this, don't bother.
  if (base::StartsWith(flag, "/prefetch:", base::CompareCase::SENSITIVE))
    return true;
#endif

  if (!base::StartsWith(flag, "--", base::CompareCase::SENSITIVE))
    return false;
  size_t end = flag.find("=");
  size_t len = (end == std::string::npos) ? flag.length() - 2 : end - 2;
  for (size_t i = 0; i < arraysize(kIgnoreSwitches); ++i) {
    if (flag.compare(2, len, kIgnoreSwitches[i]) == 0)
      return true;
  }
  return false;
}

void SetCrashKeysFromCommandLine(const base::CommandLine& command_line) {
  return SetSwitchesFromCommandLine(command_line, &IsBoringSwitch);
}

void SetActiveExtensions(const std::set<std::string>& extensions) {
  base::debug::SetCrashKeyValue(kNumExtensionsCount,
      base::StringPrintf("%" PRIuS, extensions.size()));

  std::set<std::string>::const_iterator it = extensions.begin();
  for (size_t i = 0; i < kExtensionIDMaxCount; ++i) {
    std::string key = base::StringPrintf(kExtensionID, i + 1);
    if (it == extensions.end()) {
      base::debug::ClearCrashKey(key);
    } else {
      base::debug::SetCrashKeyValue(key, *it);
      ++it;
    }
  }
}

ScopedPrinterInfo::ScopedPrinterInfo(const base::StringPiece& data) {
  std::vector<std::string> info = base::SplitString(
      data.as_string(), ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t i = 0; i < kPrinterInfoCount; ++i) {
    std::string key = base::StringPrintf(kPrinterInfo, i + 1);
    std::string value;
    if (i < info.size())
      value = info[i];
    base::debug::SetCrashKeyValue(key, value);
  }
}

ScopedPrinterInfo::~ScopedPrinterInfo() {
  for (size_t i = 0; i < kPrinterInfoCount; ++i) {
    std::string key = base::StringPrintf(kPrinterInfo, i + 1);
    base::debug::ClearCrashKey(key);
  }
}

}  // namespace crash_keys
