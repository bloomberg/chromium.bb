// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_switches.h"

#if defined(OS_MACOSX)
#include "breakpad/src/common/simple_string_dictionary.h"
#elif defined(OS_WIN)
#include "breakpad/src/client/windows/common/ipc_protocol.h"
#elif defined(OS_CHROMEOS)
#include "chrome/common/chrome_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_switches.h"
#endif

namespace crash_keys {

// The maximum lengths specified by breakpad include the trailing NULL, so
// the actual length of the string is one less.
#if defined(OS_MACOSX)
static const size_t kSingleChunkLength =
    google_breakpad::SimpleStringDictionary::value_size - 1;
#elif defined(OS_WIN)
static const size_t kSingleChunkLength =
    google_breakpad::CustomInfoEntry::kValueMaxLength - 1;
#else
static const size_t kSingleChunkLength = 63;
#endif

// Guarantees for crash key sizes.
static_assert(kSmallSize <= kSingleChunkLength,
              "crash key chunk size too small");
#if defined(OS_MACOSX)
static_assert(kMediumSize <= kSingleChunkLength,
              "mac has medium size crash key chunks");
#endif

const char kActiveURL[] = "url-chunk";

const char kFontKeyName[] = "font_key_name";

const char kSwitch[] = "switch-%" PRIuS;
const char kNumSwitches[] = "num-switches";

const char kExtensionID[] = "extension-%" PRIuS;
const char kNumExtensionsCount[] = "num-extensions";

const char kShutdownType[] = "shutdown-type";

#if !defined(OS_ANDROID)
const char kGPUVendorID[] = "gpu-venid";
const char kGPUDeviceID[] = "gpu-devid";
#endif
const char kGPUDriverVersion[] = "gpu-driver";
const char kGPUPixelShaderVersion[] = "gpu-psver";
const char kGPUVertexShaderVersion[] = "gpu-vsver";
#if defined(OS_MACOSX)
const char kGPUGLVersion[] = "gpu-glver";
#elif defined(OS_POSIX)
const char kGPUVendor[] = "gpu-gl-vendor";
const char kGPURenderer[] = "gpu-gl-renderer";
#endif

const char kPrinterInfo[] = "prn-info-%" PRIuS;

#if defined(OS_CHROMEOS)
const char kNumberOfUsers[] = "num-users";
#endif

#if defined(OS_MACOSX)
namespace mac {

const char kFirstNSException[] = "firstexception";
const char kFirstNSExceptionTrace[] = "firstexception_bt";

const char kLastNSException[] = "lastexception";
const char kLastNSExceptionTrace[] = "lastexception_bt";

const char kNSException[] = "nsexception";
const char kNSExceptionTrace[] = "nsexception_bt";

const char kSendAction[] = "sendaction";

}  // namespace mac
#endif

#if defined(KASKO)
const char kKaskoGuid[] = "kasko-guid";
const char kKaskoEquivalentGuid[] = "kasko-equivalent-guid";
#endif

const char kViewCount[] = "view-count";

size_t RegisterChromeCrashKeys() {
  // The following keys may be chunked by the underlying crash logging system,
  // but ultimately constitute a single key-value pair.
  base::debug::CrashKey fixed_keys[] = {
#if defined(OS_MACOSX)
    { kMetricsClientId, kSmallSize },
#else
    { kClientId, kSmallSize },
#endif
    { kChannel, kSmallSize },
    { kActiveURL, kLargeSize },
    { kNumSwitches, kSmallSize },
    { kNumVariations, kSmallSize },
    { kVariations, kLargeSize },
    { kNumExtensionsCount, kSmallSize },
    { kShutdownType, kSmallSize },
#if !defined(OS_ANDROID)
    { kGPUVendorID, kSmallSize },
    { kGPUDeviceID, kSmallSize },
#endif
    { kGPUDriverVersion, kSmallSize },
    { kGPUPixelShaderVersion, kSmallSize },
    { kGPUVertexShaderVersion, kSmallSize },
#if defined(OS_MACOSX)
    { kGPUGLVersion, kSmallSize },
#elif defined(OS_POSIX)
    { kGPUVendor, kSmallSize },
    { kGPURenderer, kSmallSize },
#endif

    // content/:
    { "discardable-memory-allocated", kSmallSize },
    { "discardable-memory-free", kSmallSize },
    { kFontKeyName, kSmallSize},
    { "ppapi_path", kMediumSize },
    { "subresource_url", kLargeSize },
    { "total-discardable-memory-allocated", kSmallSize },
#if defined(OS_CHROMEOS)
    { kNumberOfUsers, kSmallSize },
#endif
#if defined(OS_MACOSX)
    { mac::kFirstNSException, kMediumSize },
    { mac::kFirstNSExceptionTrace, kMediumSize },
    { mac::kLastNSException, kMediumSize },
    { mac::kLastNSExceptionTrace, kMediumSize },
    { mac::kNSException, kMediumSize },
    { mac::kNSExceptionTrace, kMediumSize },
    { mac::kSendAction, kMediumSize },
    { mac::kZombie, kMediumSize },
    { mac::kZombieTrace, kMediumSize },
    // content/:
    { "channel_error_bt", kMediumSize },
    { "remove_route_bt", kMediumSize },
    { "rwhvm_window", kMediumSize },
    // media/:
    { "VideoCaptureDeviceQTKit", kSmallSize },
#endif
#if defined(KASKO)
    { kKaskoGuid, kSmallSize },
    { kKaskoEquivalentGuid, kSmallSize },
#endif
    { kBug464926CrashKey, kSmallSize },
    { kViewCount, kSmallSize },
  };

  // This dynamic set of keys is used for sets of key value pairs when gathering
  // a collection of data, like command line switches or extension IDs.
  std::vector<base::debug::CrashKey> keys(
      fixed_keys, fixed_keys + arraysize(fixed_keys));

  // Register the switches.
  {
    // The fixed_keys names are string constants. Use static storage for
    // formatted key names as well, since they will persist for the duration of
    // the program.
    static char formatted_keys[kSwitchesMaxCount][sizeof(kSwitch) + 1] =
        {{ 0 }};
    const size_t formatted_key_len = sizeof(formatted_keys[0]);
    for (size_t i = 0; i < kSwitchesMaxCount; ++i) {
      // Name the keys using 1-based indexing.
      int n = base::snprintf(
          formatted_keys[i], formatted_key_len, kSwitch, i + 1);
      DCHECK_GT(n, 0);
      base::debug::CrashKey crash_key = { formatted_keys[i], kSmallSize };
      keys.push_back(crash_key);
    }
  }

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

  return base::debug::InitCrashKeys(&keys.at(0), keys.size(),
                                    kSingleChunkLength);
}

static bool IsBoringSwitch(const std::string& flag) {
  static const char* const kIgnoreSwitches[] = {
    switches::kEnableLogging,
    switches::kFlagSwitchesBegin,
    switches::kFlagSwitchesEnd,
    switches::kLoggingLevel,
#if defined(OS_WIN)
    // This file is linked into both chrome.dll and chrome.exe. However //ipc
    // is only in the .dll, so this needs to be a literal rather than the
    // constant.
    "channel",  // switches::kProcessChannelID
#else
    switches::kProcessChannelID,
#endif
    switches::kProcessType,
    switches::kV,
    switches::kVModule,
#if defined(OS_WIN)
    switches::kForceFieldTrials,
    switches::kPluginPath,
#elif defined(OS_MACOSX)
    switches::kMetricsClientID,
#elif defined(OS_CHROMEOS)
    switches::kPpapiFlashArgs,
    switches::kPpapiFlashPath,
    switches::kRegisterPepperPlugins,
    switches::kUIPrioritizeInGpuProcess,
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

void SetSwitchesFromCommandLine(const base::CommandLine* command_line) {
  DCHECK(command_line);
  if (!command_line)
    return;

  const base::CommandLine::StringVector& argv = command_line->argv();

  // Set the number of switches in case size > kNumSwitches.
  base::debug::SetCrashKeyValue(kNumSwitches,
      base::StringPrintf("%" PRIuS, argv.size() - 1));

  size_t key_i = 1;  // Key names are 1-indexed.

  // Go through the argv, skipping the exec path.
  for (size_t i = 1; i < argv.size(); ++i) {
#if defined(OS_WIN)
    std::string switch_str = base::WideToUTF8(argv[i]);
#else
    std::string switch_str = argv[i];
#endif

    // Skip uninteresting switches.
    if (IsBoringSwitch(switch_str))
      continue;

    // Stop if there are too many switches.
    if (i > crash_keys::kSwitchesMaxCount)
      break;

    std::string key = base::StringPrintf(kSwitch, key_i++);
    base::debug::SetCrashKeyValue(key, switch_str);
  }

  // Clear any remaining switches.
  for (; key_i <= kSwitchesMaxCount; ++key_i) {
    base::debug::ClearCrashKey(base::StringPrintf(kSwitch, key_i));
  }
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
