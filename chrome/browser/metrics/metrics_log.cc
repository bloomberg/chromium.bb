// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_log.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/cpu.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/profiler/alternate_timer.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/third_party/nspr/prtime.h"
#include "base/time/time.h"
#include "base/tracked_objects.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/metrics/extension_metrics.h"
#include "chrome/browser/omnibox/omnibox_log.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/metrics/proto/omnibox_event.pb.h"
#include "chrome/common/metrics/proto/profiler_event.pb.h"
#include "chrome/common/metrics/proto/system_profile.pb.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/nacl/common/nacl_process_type.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_client.h"
#include "content/public/common/webplugininfo.h"
#include "gpu/config/gpu_info.h"
#include "ui/gfx/screen.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "base/win/metro.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/metrics/metrics_log_chromeos.h"
#endif  // OS_CHROMEOS

using content::GpuDataManager;
using metrics::OmniboxEventProto;
using metrics::ProfilerEventProto;
using metrics::SystemProfileProto;
using tracked_objects::ProcessDataSnapshot;
typedef chrome_variations::ActiveGroupId ActiveGroupId;
typedef SystemProfileProto::GoogleUpdate::ProductInfo ProductInfo;

namespace {

// Returns the date at which the current metrics client ID was created as
// a string containing seconds since the epoch, or "0" if none was found.
std::string GetMetricsEnabledDate(PrefService* pref) {
  if (!pref) {
    NOTREACHED();
    return "0";
  }

  return pref->GetString(prefs::kMetricsReportingEnabledTimestamp);
}

OmniboxEventProto::InputType AsOmniboxEventInputType(
    AutocompleteInput::Type type) {
  switch (type) {
    case AutocompleteInput::INVALID:
      return OmniboxEventProto::INVALID;
    case AutocompleteInput::UNKNOWN:
      return OmniboxEventProto::UNKNOWN;
    case AutocompleteInput::URL:
      return OmniboxEventProto::URL;
    case AutocompleteInput::QUERY:
      return OmniboxEventProto::QUERY;
    case AutocompleteInput::FORCED_QUERY:
      return OmniboxEventProto::FORCED_QUERY;
    default:
      NOTREACHED();
      return OmniboxEventProto::INVALID;
  }
}

OmniboxEventProto::Suggestion::ResultType AsOmniboxEventResultType(
    AutocompleteMatch::Type type) {
  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
      return OmniboxEventProto::Suggestion::URL_WHAT_YOU_TYPED;
    case AutocompleteMatchType::HISTORY_URL:
      return OmniboxEventProto::Suggestion::HISTORY_URL;
    case AutocompleteMatchType::HISTORY_TITLE:
      return OmniboxEventProto::Suggestion::HISTORY_TITLE;
    case AutocompleteMatchType::HISTORY_BODY:
      return OmniboxEventProto::Suggestion::HISTORY_BODY;
    case AutocompleteMatchType::HISTORY_KEYWORD:
      return OmniboxEventProto::Suggestion::HISTORY_KEYWORD;
    case AutocompleteMatchType::NAVSUGGEST:
      return OmniboxEventProto::Suggestion::NAVSUGGEST;
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
      return OmniboxEventProto::Suggestion::SEARCH_WHAT_YOU_TYPED;
    case AutocompleteMatchType::SEARCH_HISTORY:
      return OmniboxEventProto::Suggestion::SEARCH_HISTORY;
    case AutocompleteMatchType::SEARCH_SUGGEST:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST;
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_ENTITY;
    case AutocompleteMatchType::SEARCH_SUGGEST_INFINITE:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_INFINITE;
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_PERSONALIZED;
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_PROFILE;
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
      return OmniboxEventProto::Suggestion::SEARCH_OTHER_ENGINE;
    case AutocompleteMatchType::EXTENSION_APP:
      return OmniboxEventProto::Suggestion::EXTENSION_APP;
    case AutocompleteMatchType::BOOKMARK_TITLE:
      return OmniboxEventProto::Suggestion::BOOKMARK_TITLE;
    default:
      NOTREACHED();
      return OmniboxEventProto::Suggestion::UNKNOWN_RESULT_TYPE;
  }
}

OmniboxEventProto::PageClassification AsOmniboxEventPageClassification(
    AutocompleteInput::PageClassification page_classification) {
  switch (page_classification) {
    case AutocompleteInput::INVALID_SPEC:
      return OmniboxEventProto::INVALID_SPEC;
    case AutocompleteInput::NTP:
      return OmniboxEventProto::NTP;
    case AutocompleteInput::BLANK:
      return OmniboxEventProto::BLANK;
    case AutocompleteInput::HOME_PAGE:
      return OmniboxEventProto::HOME_PAGE;
    case AutocompleteInput::OTHER:
      return OmniboxEventProto::OTHER;
    case AutocompleteInput::SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT:
      return OmniboxEventProto::
          SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT;
    case AutocompleteInput::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS:
      return OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS;
    case AutocompleteInput::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS:
      return OmniboxEventProto::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS;
    case AutocompleteInput::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT:
      return OmniboxEventProto::
          SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT;
  }
  return OmniboxEventProto::INVALID_SPEC;
}

ProfilerEventProto::TrackedObject::ProcessType AsProtobufProcessType(
    int process_type) {
  switch (process_type) {
    case content::PROCESS_TYPE_BROWSER:
      return ProfilerEventProto::TrackedObject::BROWSER;
    case content::PROCESS_TYPE_RENDERER:
      return ProfilerEventProto::TrackedObject::RENDERER;
    case content::PROCESS_TYPE_PLUGIN:
      return ProfilerEventProto::TrackedObject::PLUGIN;
    case content::PROCESS_TYPE_WORKER:
      return ProfilerEventProto::TrackedObject::WORKER;
    case content::PROCESS_TYPE_UTILITY:
      return ProfilerEventProto::TrackedObject::UTILITY;
    case content::PROCESS_TYPE_ZYGOTE:
      return ProfilerEventProto::TrackedObject::ZYGOTE;
    case content::PROCESS_TYPE_SANDBOX_HELPER:
      return ProfilerEventProto::TrackedObject::SANDBOX_HELPER;
    case content::PROCESS_TYPE_GPU:
      return ProfilerEventProto::TrackedObject::GPU;
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      return ProfilerEventProto::TrackedObject::PPAPI_PLUGIN;
    case content::PROCESS_TYPE_PPAPI_BROKER:
      return ProfilerEventProto::TrackedObject::PPAPI_BROKER;
    case PROCESS_TYPE_NACL_LOADER:
      return ProfilerEventProto::TrackedObject::NACL_LOADER;
    case PROCESS_TYPE_NACL_BROKER:
      return ProfilerEventProto::TrackedObject::NACL_BROKER;
    default:
      NOTREACHED();
      return ProfilerEventProto::TrackedObject::UNKNOWN;
  }
}

// Computes a SHA-1 hash of |data| and returns it as a hex string.
std::string ComputeSHA1(const std::string& data) {
  const std::string sha1 = base::SHA1HashString(data);
  return base::HexEncode(sha1.data(), sha1.size());
}

#if defined(ENABLE_PLUGINS)
// Returns the plugin preferences corresponding for this user, if available.
// If multiple user profiles are loaded, returns the preferences corresponding
// to an arbitrary one of the profiles.
PluginPrefs* GetPluginPrefs() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  if (!profile_manager) {
    // The profile manager can be NULL when testing.
    return NULL;
  }

  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  if (profiles.empty())
    return NULL;

  return PluginPrefs::GetForProfile(profiles.front()).get();
}

// Fills |plugin| with the info contained in |plugin_info| and |plugin_prefs|.
void SetPluginInfo(const content::WebPluginInfo& plugin_info,
                   const PluginPrefs* plugin_prefs,
                   SystemProfileProto::Plugin* plugin) {
  plugin->set_name(base::UTF16ToUTF8(plugin_info.name));
  plugin->set_filename(plugin_info.path.BaseName().AsUTF8Unsafe());
  plugin->set_version(base::UTF16ToUTF8(plugin_info.version));
  plugin->set_is_pepper(plugin_info.is_pepper_plugin());
  if (plugin_prefs)
    plugin->set_is_disabled(!plugin_prefs->IsPluginEnabled(plugin_info));
}
#endif  // defined(ENABLE_PLUGINS)

void WriteFieldTrials(const std::vector<ActiveGroupId>& field_trial_ids,
                      SystemProfileProto* system_profile) {
  for (std::vector<ActiveGroupId>::const_iterator it =
       field_trial_ids.begin(); it != field_trial_ids.end(); ++it) {
    SystemProfileProto::FieldTrial* field_trial =
        system_profile->add_field_trial();
    field_trial->set_name_id(it->name);
    field_trial->set_group_id(it->group);
  }
}

// Maps a thread name by replacing trailing sequence of digits with "*".
// Examples:
// 1. "BrowserBlockingWorker1/23857" => "BrowserBlockingWorker1/*"
// 2. "Chrome_IOThread" => "Chrome_IOThread"
std::string MapThreadName(const std::string& thread_name) {
  size_t i = thread_name.length();

  while (i > 0 && isdigit(thread_name[i - 1])) {
    --i;
  }

  if (i == thread_name.length())
    return thread_name;

  return thread_name.substr(0, i) + '*';
}

// Normalizes a source filename (which is platform- and build-method-dependent)
// by extracting the last component of the full file name.
// Example: "c:\b\build\slave\win\build\src\chrome\app\chrome_main.cc" =>
// "chrome_main.cc".
std::string NormalizeFileName(const std::string& file_name) {
  const size_t offset = file_name.find_last_of("\\/");
  return offset != std::string::npos ? file_name.substr(offset + 1) : file_name;
}

void WriteProfilerData(const ProcessDataSnapshot& profiler_data,
                       int process_type,
                       ProfilerEventProto* performance_profile) {
  for (std::vector<tracked_objects::TaskSnapshot>::const_iterator it =
           profiler_data.tasks.begin();
       it != profiler_data.tasks.end(); ++it) {
    const tracked_objects::DeathDataSnapshot& death_data = it->death_data;
    ProfilerEventProto::TrackedObject* tracked_object =
        performance_profile->add_tracked_object();
    tracked_object->set_birth_thread_name_hash(
        MetricsLogBase::Hash(MapThreadName(it->birth.thread_name)));
    tracked_object->set_exec_thread_name_hash(
        MetricsLogBase::Hash(MapThreadName(it->death_thread_name)));
    tracked_object->set_source_file_name_hash(
        MetricsLogBase::Hash(NormalizeFileName(it->birth.location.file_name)));
    tracked_object->set_source_function_name_hash(
        MetricsLogBase::Hash(it->birth.location.function_name));
    tracked_object->set_source_line_number(it->birth.location.line_number);
    tracked_object->set_exec_count(death_data.count);
    tracked_object->set_exec_time_total(death_data.run_duration_sum);
    tracked_object->set_exec_time_sampled(death_data.run_duration_sample);
    tracked_object->set_queue_time_total(death_data.queue_duration_sum);
    tracked_object->set_queue_time_sampled(death_data.queue_duration_sample);
    tracked_object->set_process_type(AsProtobufProcessType(process_type));
    tracked_object->set_process_id(profiler_data.process_id);
  }
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_WIN)
void ProductDataToProto(const GoogleUpdateSettings::ProductData& product_data,
                        ProductInfo* product_info) {
  product_info->set_version(product_data.version);
  product_info->set_last_update_success_timestamp(
      product_data.last_success.ToTimeT());
  product_info->set_last_error(product_data.last_error_code);
  product_info->set_last_extra_error(product_data.last_extra_code);
  if (ProductInfo::InstallResult_IsValid(product_data.last_result)) {
    product_info->set_last_result(
        static_cast<ProductInfo::InstallResult>(product_data.last_result));
  }
}
#endif

#if defined(OS_WIN)
struct ScreenDPIInformation {
  double max_dpi_x;
  double max_dpi_y;
};

// Called once for each connected monitor.
BOOL CALLBACK GetMonitorDPICallback(HMONITOR, HDC hdc, LPRECT, LPARAM dwData) {
  const double kMillimetersPerInch = 25.4;
  ScreenDPIInformation* screen_info =
      reinterpret_cast<ScreenDPIInformation*>(dwData);
  // Size of screen, in mm.
  DWORD size_x = GetDeviceCaps(hdc, HORZSIZE);
  DWORD size_y = GetDeviceCaps(hdc, VERTSIZE);
  double dpi_x = (size_x > 0) ?
      GetDeviceCaps(hdc, HORZRES) / (size_x / kMillimetersPerInch) : 0;
  double dpi_y = (size_y > 0) ?
      GetDeviceCaps(hdc, VERTRES) / (size_y / kMillimetersPerInch) : 0;
  screen_info->max_dpi_x = std::max(dpi_x, screen_info->max_dpi_x);
  screen_info->max_dpi_y = std::max(dpi_y, screen_info->max_dpi_y);
  return TRUE;
}

void WriteScreenDPIInformationProto(SystemProfileProto::Hardware* hardware) {
  HDC desktop_dc = GetDC(NULL);
  if (desktop_dc) {
    ScreenDPIInformation si = {0, 0};
    if (EnumDisplayMonitors(desktop_dc, NULL, GetMonitorDPICallback,
            reinterpret_cast<LPARAM>(&si))) {
      hardware->set_max_dpi_x(si.max_dpi_x);
      hardware->set_max_dpi_y(si.max_dpi_y);
    }
    ReleaseDC(GetDesktopWindow(), desktop_dc);
  }
}

#endif  // defined(OS_WIN)

// Round a timestamp measured in seconds since epoch to one with a granularity
// of an hour. This can be used before uploaded potentially sensitive
// timestamps.
int64 RoundSecondsToHour(int64 time_in_seconds) {
  return 3600 * (time_in_seconds / 3600);
}

}  // namespace

GoogleUpdateMetrics::GoogleUpdateMetrics() : is_system_install(false) {}

GoogleUpdateMetrics::~GoogleUpdateMetrics() {}

static base::LazyInstance<std::string>::Leaky
    g_version_extension = LAZY_INSTANCE_INITIALIZER;

MetricsLog::MetricsLog(const std::string& client_id, int session_id)
    : MetricsLogBase(client_id, session_id, MetricsLog::GetVersionString()),
      creation_time_(base::TimeTicks::Now()),
      extension_metrics_(uma_proto()->client_id()) {
#if defined(OS_CHROMEOS)
  metrics_log_chromeos_.reset(new MetricsLogChromeOS(uma_proto()));
#endif  // OS_CHROMEOS
}

MetricsLog::~MetricsLog() {}

// static
void MetricsLog::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kStabilityPluginStats);
}

// static
std::string MetricsLog::GetVersionString() {
  chrome::VersionInfo version_info;
  if (!version_info.is_valid()) {
    NOTREACHED() << "Unable to retrieve version info.";
    return std::string();
  }

  std::string version = version_info.Version();
  if (!version_extension().empty())
    version += version_extension();
  if (!version_info.IsOfficialBuild())
    version.append("-devel");
  return version;
}

// static
void MetricsLog::set_version_extension(const std::string& extension) {
  g_version_extension.Get() = extension;
}

// static
const std::string& MetricsLog::version_extension() {
  return g_version_extension.Get();
}

void MetricsLog::RecordStabilityMetrics(base::TimeDelta incremental_uptime,
                                        base::TimeDelta uptime,
                                        LogType log_type) {
  DCHECK_NE(NO_LOG, log_type);
  DCHECK(!locked());
  DCHECK(HasEnvironment());
  DCHECK(!HasStabilityMetrics());

  PrefService* pref = GetPrefService();
  DCHECK(pref);

  // Get stability attributes out of Local State, zeroing out stored values.
  // NOTE: This could lead to some data loss if this report isn't successfully
  //       sent, but that's true for all the metrics.

  WriteRequiredStabilityAttributes(pref);
  WritePluginStabilityElements(pref);

  // Record recent delta for critical stability metrics.  We can't wait for a
  // restart to gather these, as that delay biases our observation away from
  // users that run happily for a looooong time.  We send increments with each
  // uma log upload, just as we send histogram data.
  WriteRealtimeStabilityAttributes(pref, incremental_uptime, uptime);

  // Omit some stats unless this is the initial stability log.
  if (log_type != INITIAL_LOG)
    return;

  int incomplete_shutdown_count =
      pref->GetInteger(prefs::kStabilityIncompleteSessionEndCount);
  pref->SetInteger(prefs::kStabilityIncompleteSessionEndCount, 0);
  int breakpad_registration_success_count =
      pref->GetInteger(prefs::kStabilityBreakpadRegistrationSuccess);
  pref->SetInteger(prefs::kStabilityBreakpadRegistrationSuccess, 0);
  int breakpad_registration_failure_count =
      pref->GetInteger(prefs::kStabilityBreakpadRegistrationFail);
  pref->SetInteger(prefs::kStabilityBreakpadRegistrationFail, 0);
  int debugger_present_count =
      pref->GetInteger(prefs::kStabilityDebuggerPresent);
  pref->SetInteger(prefs::kStabilityDebuggerPresent, 0);
  int debugger_not_present_count =
      pref->GetInteger(prefs::kStabilityDebuggerNotPresent);
  pref->SetInteger(prefs::kStabilityDebuggerNotPresent, 0);

  // TODO(jar): The following are all optional, so we *could* optimize them for
  // values of zero (and not include them).
  SystemProfileProto::Stability* stability =
      uma_proto()->mutable_system_profile()->mutable_stability();
  stability->set_incomplete_shutdown_count(incomplete_shutdown_count);
  stability->set_breakpad_registration_success_count(
      breakpad_registration_success_count);
  stability->set_breakpad_registration_failure_count(
      breakpad_registration_failure_count);
  stability->set_debugger_present_count(debugger_present_count);
  stability->set_debugger_not_present_count(debugger_not_present_count);
}

PrefService* MetricsLog::GetPrefService() {
  return g_browser_process->local_state();
}

gfx::Size MetricsLog::GetScreenSize() const {
  return gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().GetSizeInPixel();
}

float MetricsLog::GetScreenDeviceScaleFactor() const {
  return gfx::Screen::GetNativeScreen()->
      GetPrimaryDisplay().device_scale_factor();
}

int MetricsLog::GetScreenCount() const {
  // TODO(scottmg): NativeScreen maybe wrong. http://crbug.com/133312
  return gfx::Screen::GetNativeScreen()->GetNumDisplays();
}

void MetricsLog::GetFieldTrialIds(
    std::vector<ActiveGroupId>* field_trial_ids) const {
  chrome_variations::GetFieldTrialActiveGroupIds(field_trial_ids);
}

bool MetricsLog::HasEnvironment() const {
  return uma_proto()->system_profile().has_uma_enabled_date();
}

bool MetricsLog::HasStabilityMetrics() const {
  return uma_proto()->system_profile().stability().has_launch_count();
}

void MetricsLog::WritePluginStabilityElements(PrefService* pref) {
  // Now log plugin stability info.
  const base::ListValue* plugin_stats_list = pref->GetList(
      prefs::kStabilityPluginStats);
  if (!plugin_stats_list)
    return;

#if defined(ENABLE_PLUGINS)
  SystemProfileProto::Stability* stability =
      uma_proto()->mutable_system_profile()->mutable_stability();
  for (base::ListValue::const_iterator iter = plugin_stats_list->begin();
       iter != plugin_stats_list->end(); ++iter) {
    if (!(*iter)->IsType(base::Value::TYPE_DICTIONARY)) {
      NOTREACHED();
      continue;
    }
    base::DictionaryValue* plugin_dict =
        static_cast<base::DictionaryValue*>(*iter);

    // Note that this search is potentially a quadratic operation, but given the
    // low number of plugins installed on a "reasonable" setup, this should be
    // fine.
    // TODO(isherman): Verify that this does not show up as a hotspot in
    // profiler runs.
    const SystemProfileProto::Plugin* system_profile_plugin = NULL;
    std::string plugin_name;
    plugin_dict->GetString(prefs::kStabilityPluginName, &plugin_name);
    const SystemProfileProto& system_profile = uma_proto()->system_profile();
    for (int i = 0; i < system_profile.plugin_size(); ++i) {
      if (system_profile.plugin(i).name() == plugin_name) {
        system_profile_plugin = &system_profile.plugin(i);
        break;
      }
    }

    if (!system_profile_plugin) {
      NOTREACHED();
      continue;
    }

    SystemProfileProto::Stability::PluginStability* plugin_stability =
        stability->add_plugin_stability();
    *plugin_stability->mutable_plugin() = *system_profile_plugin;

    int launches = 0;
    plugin_dict->GetInteger(prefs::kStabilityPluginLaunches, &launches);
    if (launches > 0)
      plugin_stability->set_launch_count(launches);

    int instances = 0;
    plugin_dict->GetInteger(prefs::kStabilityPluginInstances, &instances);
    if (instances > 0)
      plugin_stability->set_instance_count(instances);

    int crashes = 0;
    plugin_dict->GetInteger(prefs::kStabilityPluginCrashes, &crashes);
    if (crashes > 0)
      plugin_stability->set_crash_count(crashes);

    int loading_errors = 0;
    plugin_dict->GetInteger(prefs::kStabilityPluginLoadingErrors,
                            &loading_errors);
    if (loading_errors > 0)
      plugin_stability->set_loading_error_count(loading_errors);
  }
#endif  // defined(ENABLE_PLUGINS)

  pref->ClearPref(prefs::kStabilityPluginStats);
}

// The server refuses data that doesn't have certain values.  crashcount and
// launchcount are currently "required" in the "stability" group.
// TODO(isherman): Stop writing these attributes specially once the migration to
// protobufs is complete.
void MetricsLog::WriteRequiredStabilityAttributes(PrefService* pref) {
  int launch_count = pref->GetInteger(prefs::kStabilityLaunchCount);
  pref->SetInteger(prefs::kStabilityLaunchCount, 0);
  int crash_count = pref->GetInteger(prefs::kStabilityCrashCount);
  pref->SetInteger(prefs::kStabilityCrashCount, 0);

  SystemProfileProto::Stability* stability =
      uma_proto()->mutable_system_profile()->mutable_stability();
  stability->set_launch_count(launch_count);
  stability->set_crash_count(crash_count);
}

void MetricsLog::WriteRealtimeStabilityAttributes(
    PrefService* pref,
    base::TimeDelta incremental_uptime,
    base::TimeDelta uptime) {
  // Update the stats which are critical for real-time stability monitoring.
  // Since these are "optional," only list ones that are non-zero, as the counts
  // are aggregated (summed) server side.

  SystemProfileProto::Stability* stability =
      uma_proto()->mutable_system_profile()->mutable_stability();
  int count = pref->GetInteger(prefs::kStabilityPageLoadCount);
  if (count) {
    stability->set_page_load_count(count);
    pref->SetInteger(prefs::kStabilityPageLoadCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityRendererCrashCount);
  if (count) {
    stability->set_renderer_crash_count(count);
    pref->SetInteger(prefs::kStabilityRendererCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityExtensionRendererCrashCount);
  if (count) {
    stability->set_extension_renderer_crash_count(count);
    pref->SetInteger(prefs::kStabilityExtensionRendererCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityRendererHangCount);
  if (count) {
    stability->set_renderer_hang_count(count);
    pref->SetInteger(prefs::kStabilityRendererHangCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityChildProcessCrashCount);
  if (count) {
    stability->set_child_process_crash_count(count);
    pref->SetInteger(prefs::kStabilityChildProcessCrashCount, 0);
  }

#if defined(OS_CHROMEOS)
  metrics_log_chromeos_->WriteRealtimeStabilityAttributes(pref);
#endif  // OS_CHROMEOS

  const uint64 incremental_uptime_sec = incremental_uptime.InSeconds();
  if (incremental_uptime_sec)
    stability->set_incremental_uptime_sec(incremental_uptime_sec);
  const uint64 uptime_sec = uptime.InSeconds();
  if (uptime_sec)
    stability->set_uptime_sec(uptime_sec);
}

void MetricsLog::WritePluginList(
    const std::vector<content::WebPluginInfo>& plugin_list) {
  DCHECK(!locked());

#if defined(ENABLE_PLUGINS)
  PluginPrefs* plugin_prefs = GetPluginPrefs();
  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();
  for (std::vector<content::WebPluginInfo>::const_iterator iter =
           plugin_list.begin();
       iter != plugin_list.end(); ++iter) {
    SystemProfileProto::Plugin* plugin = system_profile->add_plugin();
    SetPluginInfo(*iter, plugin_prefs, plugin);
  }
#endif  // defined(ENABLE_PLUGINS)
}

void MetricsLog::RecordEnvironment(
    const std::vector<content::WebPluginInfo>& plugin_list,
    const GoogleUpdateMetrics& google_update_metrics,
    const std::vector<chrome_variations::ActiveGroupId>& synthetic_trials) {
  DCHECK(!HasEnvironment());

  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();

  std::string brand_code;
  if (google_util::GetBrand(&brand_code))
    system_profile->set_brand_code(brand_code);

  int enabled_date;
  bool success = base::StringToInt(GetMetricsEnabledDate(GetPrefService()),
                                   &enabled_date);
  DCHECK(success);

  // Reduce granularity of the enabled_date field to nearest hour.
  system_profile->set_uma_enabled_date(RoundSecondsToHour(enabled_date));

  int64 install_date = GetPrefService()->GetInt64(prefs::kInstallDate);

  // Reduce granularity of the install_date field to nearest hour.
  system_profile->set_install_date(RoundSecondsToHour(install_date));

  system_profile->set_application_locale(
      g_browser_process->GetApplicationLocale());

  SystemProfileProto::Hardware* hardware = system_profile->mutable_hardware();
  hardware->set_cpu_architecture(base::SysInfo::OperatingSystemArchitecture());
  hardware->set_system_ram_mb(base::SysInfo::AmountOfPhysicalMemoryMB());
#if defined(OS_WIN)
  hardware->set_dll_base(reinterpret_cast<uint64>(&__ImageBase));
#endif

  SystemProfileProto::Network* network = system_profile->mutable_network();
  network->set_connection_type_is_ambiguous(
      network_observer_.connection_type_is_ambiguous());
  network->set_connection_type(network_observer_.connection_type());
  network->set_wifi_phy_layer_protocol_is_ambiguous(
      network_observer_.wifi_phy_layer_protocol_is_ambiguous());
  network->set_wifi_phy_layer_protocol(
      network_observer_.wifi_phy_layer_protocol());
  network_observer_.Reset();

  SystemProfileProto::OS* os = system_profile->mutable_os();
  std::string os_name = base::SysInfo::OperatingSystemName();
#if defined(OS_WIN)
  // TODO(mad): This only checks whether the main process is a Metro process at
  // upload time; not whether the collected metrics were all gathered from
  // Metro.  This is ok as an approximation for now, since users will rarely be
  // switching from Metro to Desktop mode; but we should re-evaluate whether we
  // can distinguish metrics more cleanly in the future: http://crbug.com/140568
  if (base::win::IsMetroProcess())
    os_name += " (Metro)";
#endif
  os->set_name(os_name);
  os->set_version(base::SysInfo::OperatingSystemVersion());
#if defined(OS_ANDROID)
  os->set_fingerprint(
      base::android::BuildInfo::GetInstance()->android_build_fp());
#endif

  base::CPU cpu_info;
  SystemProfileProto::Hardware::CPU* cpu = hardware->mutable_cpu();
  cpu->set_vendor_name(cpu_info.vendor_name());
  cpu->set_signature(cpu_info.signature());

  const gpu::GPUInfo& gpu_info =
      GpuDataManager::GetInstance()->GetGPUInfo();
  SystemProfileProto::Hardware::Graphics* gpu = hardware->mutable_gpu();
  gpu->set_vendor_id(gpu_info.gpu.vendor_id);
  gpu->set_device_id(gpu_info.gpu.device_id);
  gpu->set_driver_version(gpu_info.driver_version);
  gpu->set_driver_date(gpu_info.driver_date);
  SystemProfileProto::Hardware::Graphics::PerformanceStatistics*
      gpu_performance = gpu->mutable_performance_statistics();
  gpu_performance->set_graphics_score(gpu_info.performance_stats.graphics);
  gpu_performance->set_gaming_score(gpu_info.performance_stats.gaming);
  gpu_performance->set_overall_score(gpu_info.performance_stats.overall);
  gpu->set_gl_vendor(gpu_info.gl_vendor);
  gpu->set_gl_renderer(gpu_info.gl_renderer);

  const gfx::Size display_size = GetScreenSize();
  hardware->set_primary_screen_width(display_size.width());
  hardware->set_primary_screen_height(display_size.height());
  hardware->set_primary_screen_scale_factor(GetScreenDeviceScaleFactor());
  hardware->set_screen_count(GetScreenCount());

#if defined(OS_WIN)
  WriteScreenDPIInformationProto(hardware);
#endif

  WriteGoogleUpdateProto(google_update_metrics);

  WritePluginList(plugin_list);
  extension_metrics_.WriteExtensionList(uma_proto()->mutable_system_profile());

  std::vector<ActiveGroupId> field_trial_ids;
  GetFieldTrialIds(&field_trial_ids);
  WriteFieldTrials(field_trial_ids, system_profile);
  WriteFieldTrials(synthetic_trials, system_profile);

#if defined(OS_CHROMEOS)
  metrics_log_chromeos_->LogChromeOSMetrics();
#endif  // OS_CHROMEOS

  std::string serialied_system_profile;
  std::string base64_system_profile;
  if (system_profile->SerializeToString(&serialied_system_profile)) {
    base::Base64Encode(serialied_system_profile, &base64_system_profile);
    PrefService* local_state = GetPrefService();
    local_state->SetString(prefs::kStabilitySavedSystemProfile,
                           base64_system_profile);
    local_state->SetString(prefs::kStabilitySavedSystemProfileHash,
                           ComputeSHA1(serialied_system_profile));
  }
}

bool MetricsLog::LoadSavedEnvironmentFromPrefs() {
  PrefService* local_state = GetPrefService();
  const std::string base64_system_profile =
      local_state->GetString(prefs::kStabilitySavedSystemProfile);
  if (base64_system_profile.empty())
    return false;

  const std::string system_profile_hash =
      local_state->GetString(prefs::kStabilitySavedSystemProfileHash);
  local_state->ClearPref(prefs::kStabilitySavedSystemProfile);
  local_state->ClearPref(prefs::kStabilitySavedSystemProfileHash);

  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();
  std::string serialied_system_profile;
  return base::Base64Decode(base64_system_profile, &serialied_system_profile) &&
         ComputeSHA1(serialied_system_profile) == system_profile_hash &&
         system_profile->ParseFromString(serialied_system_profile);
}

void MetricsLog::RecordProfilerData(
    const tracked_objects::ProcessDataSnapshot& process_data,
    int process_type) {
  DCHECK(!locked());

  if (tracked_objects::GetTimeSourceType() !=
          tracked_objects::TIME_SOURCE_TYPE_WALL_TIME) {
    // We currently only support the default time source, wall clock time.
    return;
  }

  ProfilerEventProto* profile;
  if (!uma_proto()->profiler_event_size()) {
    // For the first process's data, add a new field to the protocol buffer.
    profile = uma_proto()->add_profiler_event();
    profile->set_profile_type(ProfilerEventProto::STARTUP_PROFILE);
    profile->set_time_source(ProfilerEventProto::WALL_CLOCK_TIME);
  } else {
    // For the remaining calls, re-use the existing field.
    profile = uma_proto()->mutable_profiler_event(0);
  }

  WriteProfilerData(process_data, process_type, profile);
}

void MetricsLog::RecordOmniboxOpenedURL(const OmniboxLog& log) {
  DCHECK(!locked());

  std::vector<base::string16> terms;
  const int num_terms =
      static_cast<int>(Tokenize(log.text, base::kWhitespaceUTF16, &terms));

  OmniboxEventProto* omnibox_event = uma_proto()->add_omnibox_event();
  omnibox_event->set_time(MetricsLogBase::GetCurrentTime());
  if (log.tab_id != -1) {
    // If we know what tab the autocomplete URL was opened in, log it.
    omnibox_event->set_tab_id(log.tab_id);
  }
  omnibox_event->set_typed_length(log.text.length());
  omnibox_event->set_just_deleted_text(log.just_deleted_text);
  omnibox_event->set_num_typed_terms(num_terms);
  omnibox_event->set_selected_index(log.selected_index);
  if (log.completed_length != base::string16::npos)
    omnibox_event->set_completed_length(log.completed_length);
  if (log.elapsed_time_since_user_first_modified_omnibox !=
      base::TimeDelta::FromMilliseconds(-1)) {
    // Only upload the typing duration if it is set/valid.
    omnibox_event->set_typing_duration_ms(
        log.elapsed_time_since_user_first_modified_omnibox.InMilliseconds());
  }
  omnibox_event->set_duration_since_last_default_match_update_ms(
      log.elapsed_time_since_last_change_to_default_match.InMilliseconds());
  omnibox_event->set_current_page_classification(
      AsOmniboxEventPageClassification(log.current_page_classification));
  omnibox_event->set_input_type(AsOmniboxEventInputType(log.input_type));
  omnibox_event->set_is_top_result_hidden_in_dropdown(
      log.result.ShouldHideTopMatch());

  for (AutocompleteResult::const_iterator i(log.result.begin());
       i != log.result.end(); ++i) {
    OmniboxEventProto::Suggestion* suggestion = omnibox_event->add_suggestion();
    suggestion->set_provider(i->provider->AsOmniboxEventProviderType());
    suggestion->set_result_type(AsOmniboxEventResultType(i->type));
    suggestion->set_relevance(i->relevance);
    if (i->typed_count != -1)
      suggestion->set_typed_count(i->typed_count);
    suggestion->set_is_starred(i->starred);
  }
  for (ProvidersInfo::const_iterator i(log.providers_info.begin());
       i != log.providers_info.end(); ++i) {
    OmniboxEventProto::ProviderInfo* provider_info =
        omnibox_event->add_provider_info();
    provider_info->CopyFrom(*i);
  }

  ++num_events_;
}

void MetricsLog::WriteGoogleUpdateProto(
    const GoogleUpdateMetrics& google_update_metrics) {
#if defined(GOOGLE_CHROME_BUILD) && defined(OS_WIN)
  SystemProfileProto::GoogleUpdate* google_update =
      uma_proto()->mutable_system_profile()->mutable_google_update();

  google_update->set_is_system_install(google_update_metrics.is_system_install);

  if (!google_update_metrics.last_started_au.is_null()) {
    google_update->set_last_automatic_start_timestamp(
        google_update_metrics.last_started_au.ToTimeT());
  }

  if (!google_update_metrics.last_checked.is_null()) {
    google_update->set_last_update_check_timestamp(
      google_update_metrics.last_checked.ToTimeT());
  }

  if (!google_update_metrics.google_update_data.version.empty()) {
    ProductDataToProto(google_update_metrics.google_update_data,
                       google_update->mutable_google_update_status());
  }

  if (!google_update_metrics.product_data.version.empty()) {
    ProductDataToProto(google_update_metrics.product_data,
                       google_update->mutable_client_status());
  }
#endif  // defined(GOOGLE_CHROME_BUILD) && defined(OS_WIN)
}
