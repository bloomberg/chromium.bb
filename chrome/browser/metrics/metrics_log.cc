// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_log.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/perftimer.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/third_party/nspr/prtime.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gpu_process_host_ui_shim.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "webkit/plugins/npapi/webplugininfo.h"

#define OPEN_ELEMENT_FOR_SCOPE(name) ScopedElement scoped_element(this, name)

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
#if defined(OS_WIN)
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

MetricsLog::MetricsLog(const std::string& client_id, int session_id)
    : MetricsLogBase(client_id, session_id, MetricsLog::GetVersionString()) {}

MetricsLog::~MetricsLog() {}

// static
void MetricsLog::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterListPref(prefs::kStabilityPluginStats);
}

int64 MetricsLog::GetIncrementalUptime(PrefService* pref) {
  base::TimeTicks now = base::TimeTicks::Now();
  static base::TimeTicks last_updated_time(now);
  int64 incremental_time = (now - last_updated_time).InSeconds();
  last_updated_time = now;

  if (incremental_time > 0) {
    int64 metrics_uptime = pref->GetInt64(prefs::kUninstallMetricsUptimeSec);
    metrics_uptime += incremental_time;
    pref->SetInt64(prefs::kUninstallMetricsUptimeSec, metrics_uptime);
  }

  return incremental_time;
}

std::string MetricsLog::GetInstallDate() const {
  PrefService* pref = g_browser_process->local_state();
  if (pref) {
    return pref->GetString(prefs::kMetricsClientIDTimestamp);
  } else {
    NOTREACHED();
    return "0";
  }
}

// static
std::string MetricsLog::GetVersionString() {
  chrome::VersionInfo version_info;
  if (!version_info.is_valid()) {
    NOTREACHED() << "Unable to retrieve version info.";
    return std::string();
  }

  std::string version = version_info.Version();
  if (!version_extension_.empty())
    version += version_extension_;
  if (!version_info.IsOfficialBuild())
    version.append("-devel");
  return version;
}

MetricsLog* MetricsLog::AsMetricsLog() {
  return this;
}

void MetricsLog::RecordIncrementalStabilityElements() {
  DCHECK(!locked_);

  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);

  OPEN_ELEMENT_FOR_SCOPE("profile");
  WriteCommonEventAttributes();

  WriteInstallElement();  // Supply appversion.

  {
    OPEN_ELEMENT_FOR_SCOPE("stability");  // Minimal set of stability elements.
    WriteRequiredStabilityAttributes(pref);
    WriteRealtimeStabilityAttributes(pref);

    WritePluginStabilityElements(pref);
  }
}

void MetricsLog::WriteStabilityElement(PrefService* pref) {
  DCHECK(!locked_);

  DCHECK(pref);

  // Get stability attributes out of Local State, zeroing out stored values.
  // NOTE: This could lead to some data loss if this report isn't successfully
  //       sent, but that's true for all the metrics.

  OPEN_ELEMENT_FOR_SCOPE("stability");
  WriteRequiredStabilityAttributes(pref);
  WriteRealtimeStabilityAttributes(pref);

  // TODO(jar): The following are all optional, so we *could* optimize them for
  // values of zero (and not include them).
  WriteIntAttribute("incompleteshutdowncount",
                    pref->GetInteger(
                        prefs::kStabilityIncompleteSessionEndCount));
  pref->SetInteger(prefs::kStabilityIncompleteSessionEndCount, 0);


  WriteIntAttribute("breakpadregistrationok",
      pref->GetInteger(prefs::kStabilityBreakpadRegistrationSuccess));
  pref->SetInteger(prefs::kStabilityBreakpadRegistrationSuccess, 0);
  WriteIntAttribute("breakpadregistrationfail",
      pref->GetInteger(prefs::kStabilityBreakpadRegistrationFail));
  pref->SetInteger(prefs::kStabilityBreakpadRegistrationFail, 0);
  WriteIntAttribute("debuggerpresent",
                   pref->GetInteger(prefs::kStabilityDebuggerPresent));
  pref->SetInteger(prefs::kStabilityDebuggerPresent, 0);
  WriteIntAttribute("debuggernotpresent",
                   pref->GetInteger(prefs::kStabilityDebuggerNotPresent));
  pref->SetInteger(prefs::kStabilityDebuggerNotPresent, 0);

  WritePluginStabilityElements(pref);
}

void MetricsLog::WritePluginStabilityElements(PrefService* pref) {
  // Now log plugin stability info.
  const ListValue* plugin_stats_list = pref->GetList(
      prefs::kStabilityPluginStats);
  if (plugin_stats_list) {
    OPEN_ELEMENT_FOR_SCOPE("plugins");
    for (ListValue::const_iterator iter = plugin_stats_list->begin();
         iter != plugin_stats_list->end(); ++iter) {
      if (!(*iter)->IsType(Value::TYPE_DICTIONARY)) {
        NOTREACHED();
        continue;
      }
      DictionaryValue* plugin_dict = static_cast<DictionaryValue*>(*iter);

      std::string plugin_name;
      plugin_dict->GetString(prefs::kStabilityPluginName, &plugin_name);

      OPEN_ELEMENT_FOR_SCOPE("pluginstability");
      // Use "filename" instead of "name", otherwise we need to update the
      // UMA servers.
      WriteAttribute("filename", CreateBase64Hash(plugin_name));

      int launches = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginLaunches, &launches);
      WriteIntAttribute("launchcount", launches);

      int instances = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginInstances, &instances);
      WriteIntAttribute("instancecount", instances);

      int crashes = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginCrashes, &crashes);
      WriteIntAttribute("crashcount", crashes);
    }

    pref->ClearPref(prefs::kStabilityPluginStats);
  }
}

void MetricsLog::WriteRequiredStabilityAttributes(PrefService* pref) {
  // The server refuses data that doesn't have certain values.  crashcount and
  // launchcount are currently "required" in the "stability" group.
  WriteIntAttribute("launchcount",
                    pref->GetInteger(prefs::kStabilityLaunchCount));
  pref->SetInteger(prefs::kStabilityLaunchCount, 0);
  WriteIntAttribute("crashcount",
                    pref->GetInteger(prefs::kStabilityCrashCount));
  pref->SetInteger(prefs::kStabilityCrashCount, 0);
}

void MetricsLog::WriteRealtimeStabilityAttributes(PrefService* pref) {
  // Update the stats which are critical for real-time stability monitoring.
  // Since these are "optional," only list ones that are non-zero, as the counts
  // are aggergated (summed) server side.

  int count = pref->GetInteger(prefs::kStabilityPageLoadCount);
  if (count) {
    WriteIntAttribute("pageloadcount", count);
    pref->SetInteger(prefs::kStabilityPageLoadCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityRendererCrashCount);
  if (count) {
    WriteIntAttribute("renderercrashcount", count);
    pref->SetInteger(prefs::kStabilityRendererCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityExtensionRendererCrashCount);
  if (count) {
    WriteIntAttribute("extensionrenderercrashcount", count);
    pref->SetInteger(prefs::kStabilityExtensionRendererCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityRendererHangCount);
  if (count) {
    WriteIntAttribute("rendererhangcount", count);
    pref->SetInteger(prefs::kStabilityRendererHangCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityChildProcessCrashCount);
  if (count) {
    WriteIntAttribute("childprocesscrashcount", count);
    pref->SetInteger(prefs::kStabilityChildProcessCrashCount, 0);
  }

#if defined(OS_CHROMEOS)
  count = pref->GetInteger(prefs::kStabilityOtherUserCrashCount);
  if (count) {
    // TODO(kmixter): Write attribute once log server supports it
    // and remove warning log.
    // WriteIntAttribute("otherusercrashcount", count);
    LOG(WARNING) << "Not yet able to send otherusercrashcount="
                 << count;
    pref->SetInteger(prefs::kStabilityOtherUserCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityKernelCrashCount);
  if (count) {
    // TODO(kmixter): Write attribute once log server supports it
    // and remove warning log.
    // WriteIntAttribute("kernelcrashcount", count);
    LOG(WARNING) << "Not yet able to send kernelcrashcount="
                 << count;
    pref->SetInteger(prefs::kStabilityKernelCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilitySystemUncleanShutdownCount);
  if (count) {
    // TODO(kmixter): Write attribute once log server supports it
    // and remove warning log.
    // WriteIntAttribute("systemuncleanshutdowns", count);
    LOG(WARNING) << "Not yet able to send systemuncleanshutdowns="
                 << count;
    pref->SetInteger(prefs::kStabilitySystemUncleanShutdownCount, 0);
  }
#endif  // OS_CHROMEOS

  int64 recent_duration = GetIncrementalUptime(pref);
  if (recent_duration)
    WriteInt64Attribute("uptimesec", recent_duration);
}

void MetricsLog::WritePluginList(
    const std::vector<webkit::npapi::WebPluginInfo>& plugin_list) {
  DCHECK(!locked_);

  OPEN_ELEMENT_FOR_SCOPE("plugins");

  for (std::vector<webkit::npapi::WebPluginInfo>::const_iterator iter =
           plugin_list.begin();
       iter != plugin_list.end(); ++iter) {
    OPEN_ELEMENT_FOR_SCOPE("plugin");

    // Plugin name and filename are hashed for the privacy of those
    // testing unreleased new extensions.
    WriteAttribute("name", CreateBase64Hash(UTF16ToUTF8(iter->name)));
    std::string filename_bytes =
#if defined(OS_WIN)
        UTF16ToUTF8(iter->path.BaseName().value());
#else
        iter->path.BaseName().value();
#endif
    WriteAttribute("filename", CreateBase64Hash(filename_bytes));
    WriteAttribute("version", UTF16ToUTF8(iter->version));
  }
}

void MetricsLog::WriteInstallElement() {
  OPEN_ELEMENT_FOR_SCOPE("install");
  WriteAttribute("installdate", GetInstallDate());
  WriteIntAttribute("buildid", 0);  // We're using appversion instead.
}

void MetricsLog::RecordEnvironment(
         const std::vector<webkit::npapi::WebPluginInfo>& plugin_list,
         const DictionaryValue* profile_metrics) {
  DCHECK(!locked_);

  PrefService* pref = g_browser_process->local_state();

  OPEN_ELEMENT_FOR_SCOPE("profile");
  WriteCommonEventAttributes();

  WriteInstallElement();

  WritePluginList(plugin_list);

  WriteStabilityElement(pref);

  {
    OPEN_ELEMENT_FOR_SCOPE("cpu");
    WriteAttribute("arch", base::SysInfo::CPUArchitecture());
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("memory");
    WriteIntAttribute("mb", base::SysInfo::AmountOfPhysicalMemoryMB());
#if defined(OS_WIN)
    WriteIntAttribute("dllbase", reinterpret_cast<int>(&__ImageBase));
#endif
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("os");
    WriteAttribute("name",
                   base::SysInfo::OperatingSystemName());
    WriteAttribute("version",
                   base::SysInfo::OperatingSystemVersion());
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("gpu");
    GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::GetForRenderer(0);
    if (ui_shim) {
      WriteIntAttribute("vendorid", ui_shim->gpu_info().vendor_id);
      WriteIntAttribute("deviceid", ui_shim->gpu_info().device_id);
    }
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("display");
    int width = 0;
    int height = 0;
    base::SysInfo::GetPrimaryDisplayDimensions(&width, &height);
    WriteIntAttribute("xsize", width);
    WriteIntAttribute("ysize", height);
    WriteIntAttribute("screens", base::SysInfo::DisplayCount());
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("bookmarks");
    int num_bookmarks_on_bookmark_bar =
        pref->GetInteger(prefs::kNumBookmarksOnBookmarkBar);
    int num_folders_on_bookmark_bar =
        pref->GetInteger(prefs::kNumFoldersOnBookmarkBar);
    int num_bookmarks_in_other_bookmarks_folder =
        pref->GetInteger(prefs::kNumBookmarksInOtherBookmarkFolder);
    int num_folders_in_other_bookmarks_folder =
        pref->GetInteger(prefs::kNumFoldersInOtherBookmarkFolder);
    {
      OPEN_ELEMENT_FOR_SCOPE("bookmarklocation");
      WriteAttribute("name", "full-tree");
      WriteIntAttribute("foldercount",
          num_folders_on_bookmark_bar + num_folders_in_other_bookmarks_folder);
      WriteIntAttribute("itemcount",
          num_bookmarks_on_bookmark_bar +
          num_bookmarks_in_other_bookmarks_folder);
    }
    {
      OPEN_ELEMENT_FOR_SCOPE("bookmarklocation");
      WriteAttribute("name", "toolbar");
      WriteIntAttribute("foldercount", num_folders_on_bookmark_bar);
      WriteIntAttribute("itemcount", num_bookmarks_on_bookmark_bar);
    }
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("keywords");
    WriteIntAttribute("count", pref->GetInteger(prefs::kNumKeywords));
  }

  if (profile_metrics)
    WriteAllProfilesMetrics(*profile_metrics);
}

void MetricsLog::WriteAllProfilesMetrics(
    const DictionaryValue& all_profiles_metrics) {
  const std::string profile_prefix(prefs::kProfilePrefix);
  for (DictionaryValue::key_iterator i = all_profiles_metrics.begin_keys();
       i != all_profiles_metrics.end_keys(); ++i) {
    const std::string& key_name = *i;
    if (key_name.compare(0, profile_prefix.size(), profile_prefix) == 0) {
      DictionaryValue* profile;
      if (all_profiles_metrics.GetDictionaryWithoutPathExpansion(key_name,
                                                                 &profile))
        WriteProfileMetrics(key_name.substr(profile_prefix.size()), *profile);
    }
  }
}

void MetricsLog::WriteProfileMetrics(const std::string& profileidhash,
                                     const DictionaryValue& profile_metrics) {
  OPEN_ELEMENT_FOR_SCOPE("userprofile");
  WriteAttribute("profileidhash", profileidhash);
  for (DictionaryValue::key_iterator i = profile_metrics.begin_keys();
       i != profile_metrics.end_keys(); ++i) {
    Value* value;
    if (profile_metrics.GetWithoutPathExpansion(*i, &value)) {
      DCHECK(*i != "id");
      switch (value->GetType()) {
        case Value::TYPE_STRING: {
          std::string string_value;
          if (value->GetAsString(&string_value)) {
            OPEN_ELEMENT_FOR_SCOPE("profileparam");
            WriteAttribute("name", *i);
            WriteAttribute("value", string_value);
          }
          break;
        }

        case Value::TYPE_BOOLEAN: {
          bool bool_value;
          if (value->GetAsBoolean(&bool_value)) {
            OPEN_ELEMENT_FOR_SCOPE("profileparam");
            WriteAttribute("name", *i);
            WriteIntAttribute("value", bool_value ? 1 : 0);
          }
          break;
        }

        case Value::TYPE_INTEGER: {
          int int_value;
          if (value->GetAsInteger(&int_value)) {
            OPEN_ELEMENT_FOR_SCOPE("profileparam");
            WriteAttribute("name", *i);
            WriteIntAttribute("value", int_value);
          }
          break;
        }

        default:
          NOTREACHED();
          break;
      }
    }
  }
}

void MetricsLog::RecordOmniboxOpenedURL(const AutocompleteLog& log) {
  DCHECK(!locked_);

  OPEN_ELEMENT_FOR_SCOPE("uielement");
  WriteAttribute("action", "autocomplete");
  WriteAttribute("targetidhash", "");
  // TODO(kochi): Properly track windows.
  WriteIntAttribute("window", 0);
  WriteCommonEventAttributes();

  {
    OPEN_ELEMENT_FOR_SCOPE("autocomplete");

    WriteIntAttribute("typedlength", static_cast<int>(log.text.length()));
    WriteIntAttribute("selectedindex", static_cast<int>(log.selected_index));
    WriteIntAttribute("completedlength",
                      static_cast<int>(log.inline_autocompleted_length));
    const std::string input_type(
        AutocompleteInput::TypeToString(log.input_type));
    if (!input_type.empty())
      WriteAttribute("inputtype", input_type);

    for (AutocompleteResult::const_iterator i(log.result.begin());
         i != log.result.end(); ++i) {
      OPEN_ELEMENT_FOR_SCOPE("autocompleteitem");
      if (i->provider)
        WriteAttribute("provider", i->provider->name());
      const std::string result_type(AutocompleteMatch::TypeToString(i->type));
      if (!result_type.empty())
        WriteAttribute("resulttype", result_type);
      WriteIntAttribute("relevance", i->relevance);
      WriteIntAttribute("isstarred", i->starred ? 1 : 0);
    }
  }

  ++num_events_;
}
