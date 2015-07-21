// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

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

// A small crash key, guaranteed to never be split into multiple pieces.
const size_t kSmallSize = 63;

// A medium crash key, which will be chunked on certain platforms but not
// others. Guaranteed to never be more than four chunks.
const size_t kMediumSize = kSmallSize * 4;

// A large crash key, which will be chunked on all platforms. This should be
// used sparingly.
const size_t kLargeSize = kSmallSize * 16;

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

#if defined(OS_MACOSX)
// Crashpad owns the "guid" key. Chrome's metrics client ID is a separate ID
// carried in a distinct "metrics_client_id" field.
const char kMetricsClientId[] = "metrics_client_id";
#else
const char kClientId[] = "guid";
#endif

const char kChannel[] = "channel";

const char kActiveURL[] = "url-chunk";

const char kFontKeyName[] = "font_key_name";

const char kSwitch[] = "switch-%" PRIuS;
const char kNumSwitches[] = "num-switches";

const char kNumVariations[] = "num-experiments";
const char kVariations[] = "variations";

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

const char kZombie[] = "zombie";
const char kZombieTrace[] = "zombie_dealloc_bt";

}  // namespace mac
#endif

#if defined(KASKO)
const char kKaskoGuid[] = "kasko-guid";
const char kKaskoEquivalentGuid[] = "kasko-equivalent-guid";
#endif

// Used to help investigate bug 464926.  NOTE: This value is defined multiple
// places in the codebase due to layering issues. DO NOT change the value here
// without changing it in all other places that it is defined in the codebase
// (search for |kBug464926CrashKey|).
const char kBug464926CrashKey[] = "bug-464926-info";

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

void SetMetricsClientIdFromGUID(const std::string& metrics_client_guid) {
  std::string stripped_guid(metrics_client_guid);
  // Remove all instance of '-' char from the GUID. So BCD-WXY becomes BCDWXY.
  base::ReplaceSubstringsAfterOffset(
      &stripped_guid, 0, "-", base::StringPiece());
  if (stripped_guid.empty())
    return;

#if defined(OS_MACOSX)
  // The crash client ID is maintained by Crashpad and is distinct from the
  // metrics client ID, which is carried in its own key.
  base::debug::SetCrashKeyValue(kMetricsClientId, stripped_guid);
#else
  // The crash client ID is set by the application when Breakpad is in use.
  // The same ID as the metrics client ID is used.
  base::debug::SetCrashKeyValue(kClientId, stripped_guid);
#endif
}

void ClearMetricsClientId() {
#if defined(OS_MACOSX)
  // Crashpad always monitors for crashes, but doesn't upload them when
  // crash reporting is disabled. The preference to upload crash reports is
  // linked to the preference for metrics reporting. When metrics reporting is
  // disabled, don't put the metrics client ID into crash dumps. This way, crash
  // reports that are saved but not uploaded will not have a metrics client ID
  // from the time that metrics reporting was disabled even if they are uploaded
  // by user action at a later date.
  //
  // Breakpad cannot be enabled or disabled without an application restart, and
  // it needs to use the metrics client ID as its stable crash client ID, so
  // leave its client ID intact even when metrics reporting is disabled while
  // the application is running.
  base::debug::ClearCrashKey(kMetricsClientId);
#endif
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

void SetVariationsList(const std::vector<std::string>& variations) {
  base::debug::SetCrashKeyValue(kNumVariations,
      base::StringPrintf("%" PRIuS, variations.size()));

  std::string variations_string;
  variations_string.reserve(kLargeSize);

  for (size_t i = 0; i < variations.size(); ++i) {
    const std::string& variation = variations[i];
    // Do not truncate an individual experiment.
    if (variations_string.size() + variation.size() >= kLargeSize)
      break;
    variations_string += variation;
    variations_string += ",";
  }

  base::debug::SetCrashKeyValue(kVariations, variations_string);
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
