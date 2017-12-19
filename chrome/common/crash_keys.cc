// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "components/crash/core/common/crash_key.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "content/public/common/content_switches.h"

#if defined(OS_CHROMEOS)
#include "chrome/common/chrome_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_switches.h"
#endif

namespace crash_keys {

const char kActiveURL[] = "url-chunk";

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
    {kActiveURL, kLargeSize},
    {kNumVariations, kSmallSize},
    {kVariations, kHugeSize},

    // TODO(sunnyps): Remove after fixing crbug.com/724999.
    {"gl-context-set-current-stack-trace", kMediumSize},
  };

  // This dynamic set of keys is used for sets of key value pairs when gathering
  // a collection of data, like command line switches or extension IDs.
  std::vector<base::debug::CrashKey> keys(
      fixed_keys, fixed_keys + arraysize(fixed_keys));

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
  static crash_reporter::CrashKeyString<4> num_extensions("num-extensions");
  num_extensions.Set(base::NumberToString(extensions.size()));

  using ExtensionIDKey = crash_reporter::CrashKeyString<64>;
  static ExtensionIDKey extension_ids[] = {
      {"extension-1", ExtensionIDKey::Tag::kArray},
      {"extension-2", ExtensionIDKey::Tag::kArray},
      {"extension-3", ExtensionIDKey::Tag::kArray},
      {"extension-4", ExtensionIDKey::Tag::kArray},
      {"extension-5", ExtensionIDKey::Tag::kArray},
      {"extension-6", ExtensionIDKey::Tag::kArray},
      {"extension-7", ExtensionIDKey::Tag::kArray},
      {"extension-8", ExtensionIDKey::Tag::kArray},
      {"extension-9", ExtensionIDKey::Tag::kArray},
      {"extension-10", ExtensionIDKey::Tag::kArray},
  };

  std::set<std::string>::const_iterator it = extensions.begin();
  for (size_t i = 0; i < arraysize(extension_ids); ++i) {
    if (it == extensions.end()) {
      extension_ids[i].Clear();
    } else {
      extension_ids[i].Set(*it);
      ++it;
    }
  }
}

using PrinterInfoKey = crash_reporter::CrashKeyString<64>;
static PrinterInfoKey printer_info_keys[] = {
    {"prn-info-1", PrinterInfoKey::Tag::kArray},
    {"prn-info-2", PrinterInfoKey::Tag::kArray},
    {"prn-info-3", PrinterInfoKey::Tag::kArray},
    {"prn-info-4", PrinterInfoKey::Tag::kArray},
};

ScopedPrinterInfo::ScopedPrinterInfo(const base::StringPiece& data) {
  std::vector<std::string> info = base::SplitString(
      data.as_string(), ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t i = 0; i < arraysize(printer_info_keys); ++i) {
    std::string value;
    if (i < info.size())
      value = info[i];
    printer_info_keys[i].Set(value);
  }
}

ScopedPrinterInfo::~ScopedPrinterInfo() {
  for (auto& crash_key : printer_info_keys) {
    crash_key.Clear();
  }
}

}  // namespace crash_keys
