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
COMPILE_ASSERT(kSmallSize <= kSingleChunkLength,
               crash_key_chunk_size_too_small);
#if defined(OS_MACOSX)
COMPILE_ASSERT(kMediumSize <= kSingleChunkLength,
               mac_has_medium_size_crash_key_chunks);
#endif

const char kClientId[] = "guid";

const char kChannel[] = "channel";

const char kActiveURL[] = "url-chunk";

const char kSwitch[] = "switch-%" PRIuS;
const char kNumSwitches[] = "num-switches";

const char kNumVariations[] = "num-experiments";
const char kVariations[] = "variations";

const char kExtensionID[] = "extension-%" PRIuS;
const char kNumExtensionsCount[] = "num-extensions";

const char kNumberOfViews[] = "num-views";

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

size_t RegisterChromeCrashKeys() {
  // The following keys may be chunked by the underlying crash logging system,
  // but ultimately constitute a single key-value pair.
  base::debug::CrashKey fixed_keys[] = {
    { kClientId, kSmallSize },
    { kChannel, kSmallSize },
    { kActiveURL, kLargeSize },
    { kNumSwitches, kSmallSize },
    { kNumVariations, kSmallSize },
    { kVariations, kLargeSize },
    { kNumExtensionsCount, kSmallSize },
    { kNumberOfViews, kSmallSize },
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

    // base/:
    { "dm-usage", kSmallSize },
    // content/:
    { "ppapi_path", kMediumSize },
    { "subresource_url", kLargeSize },
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

void SetCrashClientIdFromGUID(const std::string& client_guid) {
  std::string stripped_guid(client_guid);
  // Remove all instance of '-' char from the GUID. So BCD-WXY becomes BCDWXY.
  ReplaceSubstringsAfterOffset(&stripped_guid, 0, "-", "");
  if (stripped_guid.empty())
    return;

  base::debug::SetCrashKeyValue(kClientId, stripped_guid);
}

static bool IsBoringSwitch(const std::string& flag) {
#if defined(OS_WIN)
  return StartsWithASCII(flag, "--channel=", true) ||

         // No point to including this since we already have a ptype field.
         StartsWithASCII(flag, "--type=", true) ||

         // Not particularly interesting
         StartsWithASCII(flag, "--flash-broker=", true) ||

         // Just about everything has this, don't bother.
         StartsWithASCII(flag, "/prefetch:", true) ||

         // We handle the plugin path separately since it is usually too big
         // to fit in the switches (limited to 63 characters).
         StartsWithASCII(flag, "--plugin-path=", true) ||

         // This is too big so we end up truncating it anyway.
         StartsWithASCII(flag, "--force-fieldtrials=", true) ||

         // These surround the flags that were added by about:flags, it lets
         // you distinguish which flags were added manually via the command
         // line versus those added through about:flags. For the most part
         // we don't care how an option was enabled, so we strip these.
         // (If you need to know can always look at the PEB).
         flag == "--flag-switches-begin" ||
         flag == "--flag-switches-end";
#elif defined(OS_CHROMEOS)
  static const char* kIgnoreSwitches[] = {
    ::switches::kEnableCompositingForFixedPosition,
    ::switches::kEnableImplSidePainting,
    ::switches::kEnableLogging,
    ::switches::kFlagSwitchesBegin,
    ::switches::kFlagSwitchesEnd,
    ::switches::kLoggingLevel,
    ::switches::kPpapiFlashArgs,
    ::switches::kPpapiFlashPath,
    ::switches::kRegisterPepperPlugins,
    ::switches::kUIPrioritizeInGpuProcess,
    ::switches::kUseGL,
    ::switches::kUserDataDir,
    ::switches::kV,
    ::switches::kVModule,
    // Cros/CC flgas are specified as raw strings to avoid dependency.
    "ash-default-wallpaper-large",
    "ash-default-wallpaper-small",
    "ash-guest-wallpaper-large",
    "ash-guest-wallpaper-small",
    "enterprise-enable-forced-re-enrollment",
    "enterprise-enrollment-initial-modulus",
    "enterprise-enrollment-modulus-limit",
    "login-profile",
    "login-user",
    "max-tiles-for-interest-area",
    "max-unused-resource-memory-usage-percentage",
    "termination-message-file",
    "use-cras",
  };
  if (!StartsWithASCII(flag, "--", true))
    return false;
  std::size_t end = flag.find("=");
  int len = (end == std::string::npos) ? flag.length() - 2 : end - 2;
  for (size_t i = 0; i < arraysize(kIgnoreSwitches); ++i) {
    if (flag.compare(2, len, kIgnoreSwitches[i]) == 0)
      return true;
  }
  return false;
#else
  return false;
#endif
}

void SetSwitchesFromCommandLine(const CommandLine* command_line) {
  DCHECK(command_line);
  if (!command_line)
    return;

  const CommandLine::StringVector& argv = command_line->argv();

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
  std::vector<std::string> info;
  base::SplitString(data.as_string(), ';', &info);
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
