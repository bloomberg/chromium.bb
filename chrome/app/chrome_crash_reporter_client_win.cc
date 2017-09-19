// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(ananta/scottmg)
// Add test coverage for Crashpad.
#include "chrome/app/chrome_crash_reporter_client_win.h"

#include <windows.h>

#include <assert.h>
#include <shellapi.h>

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/leak_annotations.h"
#include "base/format_macros.h"
#include "base/rand_util.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/user_data_dir.h"
#include "components/crash/content/app/crashpad.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/version_info/channel.h"
#include "gpu/config/gpu_crash_keys.h"

namespace {

// TODO(ananta)
// When the new crash key map implementation lands, we should remove the
// constants defined below, the RegisterCrashKeysHelper function, the
// RegisterCrashKeys function in the crash_keys::CrashReporterClient interface
// and the snprintf function defined here.
constexpr char kActiveURL[] = "url-chunk";
constexpr char kFontKeyName[] = "font_key_name";

// Installed extensions. |kExtensionID| should be formatted with an integer,
// in the range [0, kExtensionIDMaxCount).
constexpr char kNumExtensionsCount[] = "num-extensions";
constexpr size_t kExtensionIDMaxCount = 10;
constexpr char kExtensionID[] = "extension-%" PRIuS;

constexpr char kShutdownType[] = "shutdown-type";
constexpr char kBrowserUnpinTrace[] = "browser-unpin-trace";

// Registry values used to determine Chrome's update channel; see
// https://crbug.com/579504.
constexpr char kApValue[] = "ap";
constexpr char kCohortName[] = "cohort-name";

constexpr char kHungRendererOutstandingAckCount[] = "hung-outstanding-acks";
constexpr char kHungRendererOutstandingEventType[] =
    "hung-outstanding-event-type";
constexpr char kHungRendererLastEventType[] = "hung-last-event-type";
constexpr char kHungRendererReason[] = "hung-reason";
constexpr char kInputEventFilterSendFailure[] =
    "input-event-filter-send-failure";

constexpr char kThirdPartyModulesLoaded[] = "third-party-modules-loaded";
constexpr char kThirdPartyModulesNotLoaded[] = "third-party-modules-not-loaded";

constexpr char kIsEnterpriseManaged[] = "is-enterprise-managed";

constexpr char kViewCount[] = "view-count";
constexpr char kZeroEncodeDetails[] = "zero-encode-details";

// The user's printers, up to kPrinterInfoCount. Should be set with
// ScopedPrinterInfo.
constexpr size_t kPrinterInfoCount = 4;
constexpr char kPrinterInfo[] = "prn-info-%" PRIuS;

using namespace crash_keys;

int snprintf(char* buffer,
             size_t size,
             _Printf_format_string_ const char* format,
             ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = vsnprintf(buffer, size, format, arguments);
  va_end(arguments);
  return result;
}

size_t RegisterCrashKeysHelper() {
  // The following keys may be chunked by the underlying crash logging system,
  // but ultimately constitute a single key-value pair.
  //
  // For now these need to be kept relatively up to date with those in
  // chrome/common/crash_keys.cc::RegisterChromeCrashKeys().
  static constexpr base::debug::CrashKey kFixedKeys[] = {
      {kMetricsClientId, kSmallSize},
      {kChannel, kSmallSize},
      {kActiveURL, kLargeSize},
      {kNumVariations, kSmallSize},
      {kVariations, kHugeSize},
      {kNumExtensionsCount, kSmallSize},
      {kShutdownType, kSmallSize},
      {kBrowserUnpinTrace, kMediumSize},
      {kApValue, kSmallSize},
      {kCohortName, kSmallSize},

      // gpu
      {gpu::crash_keys::kGPUVendorID, kSmallSize},
      {gpu::crash_keys::kGPUDeviceID, kSmallSize},
      {gpu::crash_keys::kGPUDriverVersion, kSmallSize},
      {gpu::crash_keys::kGPUPixelShaderVersion, kSmallSize},
      {gpu::crash_keys::kGPUVertexShaderVersion, kSmallSize},
      {gpu::crash_keys::kGPUGLContextIsVirtual, kSmallSize},

      // browser/:
      {kThirdPartyModulesLoaded, kSmallSize},
      {kThirdPartyModulesNotLoaded, kSmallSize},
      {kIsEnterpriseManaged, kSmallSize},

      // content/:
      {"bad_message_reason", kSmallSize},
      {"discardable-memory-allocated", kSmallSize},
      {"discardable-memory-free", kSmallSize},
      {kFontKeyName, kSmallSize},
      {"mojo-message-error", kMediumSize},
      {"ppapi_path", kMediumSize},
      {"subresource_url", kLargeSize},
      {"total-discardable-memory-allocated", kSmallSize},
      {kBug464926CrashKey, kSmallSize},
      {kViewCount, kSmallSize},
      {kHungRendererOutstandingAckCount, kSmallSize},
      {kHungRendererOutstandingEventType, kSmallSize},
      {kHungRendererLastEventType, kSmallSize},
      {kHungRendererReason, kSmallSize},
      {kInputEventFilterSendFailure, kSmallSize},

      // media/:
      {kZeroEncodeDetails, kSmallSize},

      // gin/:
      {"v8-ignition", kSmallSize},

      // Temporary for https://crbug.com/626802.
      {"newframe_routing_id", kSmallSize},
      {"newframe_proxy_id", kSmallSize},
      {"newframe_opener_id", kSmallSize},
      {"newframe_parent_id", kSmallSize},
      {"newframe_widget_id", kSmallSize},
      {"newframe_widget_hidden", kSmallSize},
      {"newframe_replicated_origin", kSmallSize},

      // Temporary for https://crbug.com/612711.
      {"aci_wrong_sp_extension_id", kSmallSize},

      // Temporary for http://crbug.com/621730
      {"postmessage_src_origin", kMediumSize},
      {"postmessage_dst_origin", kMediumSize},
      {"postmessage_dst_url", kLargeSize},
      {"postmessage_script_info", kLargeSize},

      // Temporary for https://crbug.com/697745.
      {"engine_params", kMediumSize},
      {"engine1_params", kMediumSize},
      {"engine2_params", kMediumSize},

      // Temporary for https://crbug.com/685996.
      {"user-cloud-policy-manager-connect-trace", kMediumSize},

      // TODO(asvitkine): Remove after fixing https://crbug.com/736675
      {"bad_histogram", kMediumSize},

      // Temporary for https://crbug.com/752914.
      {"blink_scheduler_task_function_name", kMediumSize},
      {"blink_scheduler_task_file_name", kMediumSize},
  };

  // This dynamic set of keys is used for sets of key value pairs when gathering
  // a collection of data, like command line switches or extension IDs.
  std::vector<base::debug::CrashKey> keys(std::begin(kFixedKeys),
                                          std::end(kFixedKeys));

  crash_keys::GetCrashKeysForCommandLineSwitches(&keys);

  // Register the extension IDs.
  {
    static char formatted_keys[kExtensionIDMaxCount]
                              [sizeof(kExtensionID) + 1] = {{0}};
    const size_t formatted_key_len = sizeof(formatted_keys[0]);
    for (size_t i = 0; i < kExtensionIDMaxCount; ++i) {
      snprintf(formatted_keys[i], formatted_key_len, kExtensionID, i + 1);
      base::debug::CrashKey crash_key = {formatted_keys[i], kSmallSize};
      keys.push_back(crash_key);
    }
  }

  // Register the printer info.
  {
    static char formatted_keys[kPrinterInfoCount]
                              [sizeof(kPrinterInfo) + 1] = {{0}};
    const size_t formatted_key_len = sizeof(formatted_keys[0]);
    for (size_t i = 0; i < kPrinterInfoCount; ++i) {
      // Key names are 1-indexed.
      snprintf(formatted_keys[i], formatted_key_len, kPrinterInfo, i + 1);
      base::debug::CrashKey crash_key = {formatted_keys[i], kSmallSize};
      keys.push_back(crash_key);
    }
  }

  return base::debug::InitCrashKeys(&keys[0], keys.size(), kChunkMaxLength);
}

}  // namespace

ChromeCrashReporterClient::ChromeCrashReporterClient() {}

ChromeCrashReporterClient::~ChromeCrashReporterClient() {}

#if !defined(NACL_WIN64)
// static
void ChromeCrashReporterClient::InitializeCrashReportingForProcess() {
  static ChromeCrashReporterClient* instance = nullptr;
  if (instance)
    return;

  instance = new ChromeCrashReporterClient();
  ANNOTATE_LEAKING_OBJECT_PTR(instance);

  std::wstring process_type = install_static::GetSwitchValueFromCommandLine(
      ::GetCommandLine(), install_static::kProcessType);
  // Don't set up Crashpad crash reporting in the Crashpad handler itself, nor
  // in the fallback crash handler for the Crashpad handler process.
  if (process_type != install_static::kCrashpadHandler &&
      process_type != install_static::kFallbackHandler) {
    crash_reporter::SetCrashReporterClient(instance);

    std::wstring user_data_dir;
    if (process_type.empty())
      install_static::GetUserDataDirectory(&user_data_dir, nullptr);

    crash_reporter::InitializeCrashpadWithEmbeddedHandler(
        process_type.empty(), install_static::UTF16ToUTF8(process_type),
        install_static::UTF16ToUTF8(user_data_dir));
  }
}
#endif  // NACL_WIN64

bool ChromeCrashReporterClient::GetAlternativeCrashDumpLocation(
    base::string16* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  *crash_dir =
      install_static::GetEnvironmentString16(L"BREAKPAD_DUMP_LOCATION");
  return !crash_dir->empty();
}

void ChromeCrashReporterClient::GetProductNameAndVersion(
    const base::string16& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
  assert(product_name);
  assert(version);
  assert(special_build);
  assert(channel_name);

  install_static::GetExecutableVersionDetails(
      exe_path, product_name, version, special_build, channel_name);
}

bool ChromeCrashReporterClient::ShouldShowRestartDialog(base::string16* title,
                                                        base::string16* message,
                                                        bool* is_rtl_locale) {
  if (!install_static::HasEnvironmentVariable16(
          install_static::kShowRestart) ||
      !install_static::HasEnvironmentVariable16(
          install_static::kRestartInfo)) {
    return false;
  }

  base::string16 restart_info =
      install_static::GetEnvironmentString16(install_static::kRestartInfo);

  // The CHROME_RESTART var contains the dialog strings separated by '|'.
  // See ChromeBrowserMainPartsWin::PrepareRestartOnCrashEnviroment()
  // for details.
  std::vector<base::string16> dlg_strings = install_static::TokenizeString16(
      restart_info, L'|', true);  // true = Trim whitespace.

  if (dlg_strings.size() < 3)
    return false;

  *title = dlg_strings[0];
  *message = dlg_strings[1];
  *is_rtl_locale = dlg_strings[2] == install_static::kRtlLocale;
  return true;
}

bool ChromeCrashReporterClient::AboutToRestart() {
  if (!install_static::HasEnvironmentVariable16(install_static::kRestartInfo))
    return false;

  install_static::SetEnvironmentString16(install_static::kShowRestart, L"1");
  return true;
}

bool ChromeCrashReporterClient::GetDeferredUploadsSupported(
    bool is_per_user_install) {
  return false;
}

bool ChromeCrashReporterClient::GetIsPerUserInstall() {
  return !install_static::IsSystemInstall();
}

bool ChromeCrashReporterClient::GetShouldDumpLargerDumps() {
  // Capture larger dumps for Google Chrome beta, dev, and canary channels, and
  // Chromium builds. The Google Chrome stable channel uses smaller dumps.
  return install_static::GetChromeChannel() != version_info::Channel::STABLE;
}

int ChromeCrashReporterClient::GetResultCodeRespawnFailed() {
  return chrome::RESULT_CODE_RESPAWN_FAILED;
}

bool ChromeCrashReporterClient::ReportingIsEnforcedByPolicy(
    bool* crashpad_enabled) {
  // Determine whether configuration management allows loading the crash
  // reporter.
  // Since the configuration management infrastructure is not initialized at
  // this point, we read the corresponding registry key directly. The return
  // status indicates whether policy data was successfully read. If it is true,
  // |breakpad_enabled| contains the value set by policy.
  return install_static::ReportingIsEnforcedByPolicy(crashpad_enabled);
}


bool ChromeCrashReporterClient::GetCrashDumpLocation(
    base::string16* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  // If this environment variable exists, then for the time being,
  // short-circuit how it's handled on Windows. Honoring this
  // variable is required in order to symbolize stack traces in
  // Telemetry based tests: http://crbug.com/561763.
  if (GetAlternativeCrashDumpLocation(crash_dir))
    return true;

  *crash_dir = install_static::GetCrashDumpLocation();
  return !crash_dir->empty();
}

bool ChromeCrashReporterClient::GetCrashMetricsLocation(
    base::string16* metrics_dir) {
  install_static::GetUserDataDirectory(metrics_dir, nullptr);
  return !metrics_dir->empty();
}

// TODO(ananta)
// This function should be removed when the new crash key map implementation
// lands.
size_t ChromeCrashReporterClient::RegisterCrashKeys() {
  return RegisterCrashKeysHelper();
}

bool ChromeCrashReporterClient::IsRunningUnattended() {
  return install_static::HasEnvironmentVariable16(install_static::kHeadless);
}

bool ChromeCrashReporterClient::GetCollectStatsConsent() {
  return install_static::GetCollectStatsConsent();
}

bool ChromeCrashReporterClient::GetCollectStatsInSample() {
  return install_static::GetCollectStatsInSample();
}

bool ChromeCrashReporterClient::ShouldMonitorCrashHandlerExpensively() {
  // The expensive mechanism dedicates a process to be crashpad_handler's own
  // crashpad_handler. In Google Chrome, scale back on this in the more stable
  // channels. There's a fallback crash handler that can catch crashes when this
  // expensive mechanism isn't used, although the fallback crash handler has
  // different characteristics so it's desirable to use the expensive mechanism
  // at least some of the time.
  double probability;
  switch (install_static::GetChromeChannel()) {
    case version_info::Channel::STABLE:
      return false;

    case version_info::Channel::BETA:
      probability = 0.1;
      break;

    case version_info::Channel::DEV:
      probability = 0.25;
      break;

    default:
      probability = 0.5;
      break;
  }

  return base::RandDouble() < probability;
}

bool ChromeCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  // This is not used by Crashpad (at least on Windows).
  NOTREACHED();
  return true;
}
