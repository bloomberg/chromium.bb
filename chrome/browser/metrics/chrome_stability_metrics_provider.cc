// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/process_map.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/metrics/plugin_metrics_provider.h"
#endif

#if defined(OS_WIN)
#include <windows.h>  // Needed for STATUS_* codes
#include "chrome/installer/util/install_util.h"
#include "components/browser_watcher/crash_reporting_metrics_win.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/memory/system_memory_stats_recorder.h"
#endif

namespace {

enum RendererType {
  RENDERER_TYPE_RENDERER = 1,
  RENDERER_TYPE_EXTENSION,
  // NOTE: Add new action types only immediately above this line. Also,
  // make sure the enum list in tools/metrics/histograms/histograms.xml is
  // updated with any change in here.
  RENDERER_TYPE_COUNT
};

// Converts an exit code into something that can be inserted into our
// histograms (which expect non-negative numbers less than MAX_INT).
int MapCrashExitCodeForHistogram(int exit_code) {
#if defined(OS_WIN)
  // Since |abs(STATUS_GUARD_PAGE_VIOLATION) == MAX_INT| it causes problems in
  // histograms.cc. Solve this by remapping it to a smaller value, which
  // hopefully doesn't conflict with other codes.
  if (exit_code == STATUS_GUARD_PAGE_VIOLATION)
    return 0x1FCF7EC3;  // Randomly picked number.
#endif

  return std::abs(exit_code);
}

#if defined(OS_WIN)
void CountBrowserCrashDumpAttempts() {
  enum Outcome {
    OUTCOME_SUCCESS,
    OUTCOME_FAILURE,
    OUTCOME_UNKNOWN,
    OUTCOME_MAX_VALUE
  };

  browser_watcher::CrashReportingMetrics::Values metrics =
      browser_watcher::CrashReportingMetrics(
          InstallUtil::IsChromeSxSProcess()
              ? chrome::kBrowserCrashDumpAttemptsRegistryPathSxS
              : chrome::kBrowserCrashDumpAttemptsRegistryPath)
          .RetrieveAndResetMetrics();

  for (int i = 0; i < metrics.crash_dump_attempts; ++i) {
    Outcome outcome = OUTCOME_UNKNOWN;
    if (i < metrics.successful_crash_dumps)
      outcome = OUTCOME_SUCCESS;
    else if (i < metrics.successful_crash_dumps + metrics.failed_crash_dumps)
      outcome = OUTCOME_FAILURE;

    UMA_STABILITY_HISTOGRAM_ENUMERATION("CrashReport.BreakpadCrashDumpOutcome",
                                        outcome, OUTCOME_MAX_VALUE);
  }

  for (int i = 0; i < metrics.dump_without_crash_attempts; ++i) {
    Outcome outcome = OUTCOME_UNKNOWN;
    if (i < metrics.successful_dumps_without_crash) {
      outcome = OUTCOME_SUCCESS;
    } else if (i < metrics.successful_dumps_without_crash +
                       metrics.failed_dumps_without_crash) {
      outcome = OUTCOME_FAILURE;
    }

    UMA_STABILITY_HISTOGRAM_ENUMERATION(
        "CrashReport.BreakpadDumpWithoutCrashOutcome", outcome,
        OUTCOME_MAX_VALUE);
  }
}
#endif  // defined(OS_WIN)

void RecordChildKills(int histogram_type) {
  UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.ChildKills",
                            histogram_type, RENDERER_TYPE_COUNT);
}

}  // namespace

ChromeStabilityMetricsProvider::ChromeStabilityMetricsProvider(
    PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
  BrowserChildProcessObserver::Add(this);
}

ChromeStabilityMetricsProvider::~ChromeStabilityMetricsProvider() {
  BrowserChildProcessObserver::Remove(this);
}

void ChromeStabilityMetricsProvider::OnRecordingEnabled() {
  registrar_.Add(this,
                 content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());
}

void ChromeStabilityMetricsProvider::OnRecordingDisabled() {
  registrar_.RemoveAll();
}

void ChromeStabilityMetricsProvider::ProvideStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  metrics::SystemProfileProto_Stability* stability_proto =
      system_profile_proto->mutable_stability();

  int count = local_state_->GetInteger(prefs::kStabilityPageLoadCount);
  if (count) {
    stability_proto->set_page_load_count(count);
    local_state_->SetInteger(prefs::kStabilityPageLoadCount, 0);
  }

  count = local_state_->GetInteger(prefs::kStabilityChildProcessCrashCount);
  if (count) {
    stability_proto->set_child_process_crash_count(count);
    local_state_->SetInteger(prefs::kStabilityChildProcessCrashCount, 0);
  }

  count = local_state_->GetInteger(prefs::kStabilityRendererCrashCount);
  if (count) {
    stability_proto->set_renderer_crash_count(count);
    local_state_->SetInteger(prefs::kStabilityRendererCrashCount, 0);
  }

  count = local_state_->GetInteger(prefs::kStabilityRendererFailedLaunchCount);
  if (count) {
    stability_proto->set_renderer_failed_launch_count(count);
    local_state_->SetInteger(prefs::kStabilityRendererFailedLaunchCount, 0);
  }

  count =
      local_state_->GetInteger(prefs::kStabilityExtensionRendererCrashCount);
  if (count) {
    stability_proto->set_extension_renderer_crash_count(count);
    local_state_->SetInteger(prefs::kStabilityExtensionRendererCrashCount, 0);
  }

  count = local_state_->GetInteger(
      prefs::kStabilityExtensionRendererFailedLaunchCount);
  if (count) {
    stability_proto->set_extension_renderer_failed_launch_count(count);
    local_state_->SetInteger(
        prefs::kStabilityExtensionRendererFailedLaunchCount, 0);
  }

  count = local_state_->GetInteger(prefs::kStabilityRendererHangCount);
  if (count) {
    stability_proto->set_renderer_hang_count(count);
    local_state_->SetInteger(prefs::kStabilityRendererHangCount, 0);
  }

#if defined(OS_WIN)
  CountBrowserCrashDumpAttempts();
#endif  // defined(OS_WIN)
}

void ChromeStabilityMetricsProvider::ClearSavedStabilityMetrics() {
  // Clear all the prefs used in this class in UMA reports (which doesn't
  // include |kUninstallMetricsPageLoadCount| as it's not sent up by UMA).
  local_state_->SetInteger(prefs::kStabilityChildProcessCrashCount, 0);
  local_state_->SetInteger(prefs::kStabilityExtensionRendererCrashCount, 0);
  local_state_->SetInteger(prefs::kStabilityExtensionRendererFailedLaunchCount,
                           0);
  local_state_->SetInteger(prefs::kStabilityPageLoadCount, 0);
  local_state_->SetInteger(prefs::kStabilityRendererCrashCount, 0);
  local_state_->SetInteger(prefs::kStabilityRendererFailedLaunchCount, 0);
  local_state_->SetInteger(prefs::kStabilityRendererHangCount, 0);
}

// static
void ChromeStabilityMetricsProvider::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kStabilityChildProcessCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityExtensionRendererCrashCount,
                                0);
  registry->RegisterIntegerPref(
      prefs::kStabilityExtensionRendererFailedLaunchCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityPageLoadCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityRendererFailedLaunchCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityRendererHangCount, 0);

  registry->RegisterInt64Pref(prefs::kUninstallMetricsPageLoadCount, 0);
}

void ChromeStabilityMetricsProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_START: {
      content::NavigationController* controller =
          content::Source<content::NavigationController>(source).ptr();
      content::WebContents* web_contents = controller->GetWebContents();
      LogLoadStarted(web_contents);
      break;
    }

    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      content::RenderProcessHost::RendererClosedDetails* process_details =
          content::Details<content::RenderProcessHost::RendererClosedDetails>(
              details).ptr();
      content::RenderProcessHost* host =
          content::Source<content::RenderProcessHost>(source).ptr();
      LogRendererCrash(
          host, process_details->status, process_details->exit_code);
      break;
    }

    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      LogRendererHang();
      break;

    default:
      NOTREACHED();
      break;
  }
}

void ChromeStabilityMetricsProvider::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    int exit_code) {
#if defined(ENABLE_PLUGINS)
  // Exclude plugin crashes from the count below because we report them via
  // a separate UMA metric.
  if (PluginMetricsProvider::IsPluginProcess(data.process_type))
    return;
#endif

  IncrementPrefValue(prefs::kStabilityChildProcessCrashCount);
}

void ChromeStabilityMetricsProvider::LogLoadStarted(
    content::WebContents* web_contents) {
  content::RecordAction(base::UserMetricsAction("PageLoad"));
  // TODO(asvitkine): Check if this is used for anything and if not, remove.
  LOCAL_HISTOGRAM_BOOLEAN("Chrome.UmaPageloadCounter", true);
  IncrementPrefValue(prefs::kStabilityPageLoadCount);
  IncrementLongPrefsValue(prefs::kUninstallMetricsPageLoadCount);
  // We need to save the prefs, as page load count is a critical stat, and it
  // might be lost due to a crash :-(.
}

void ChromeStabilityMetricsProvider::LogRendererCrash(
    content::RenderProcessHost* host,
    base::TerminationStatus status,
    int exit_code) {
  int histogram_type = RENDERER_TYPE_RENDERER;
  bool was_extension_process = false;
#if defined(ENABLE_EXTENSIONS)
  if (extensions::ProcessMap::Get(host->GetBrowserContext())
          ->Contains(host->GetID())) {
    histogram_type = RENDERER_TYPE_EXTENSION;
    was_extension_process = true;
  }
#endif
  if (status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
      status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION) {
    if (was_extension_process) {
      IncrementPrefValue(prefs::kStabilityExtensionRendererCrashCount);

      UMA_HISTOGRAM_SPARSE_SLOWLY("CrashExitCodes.Extension",
                                  MapCrashExitCodeForHistogram(exit_code));
    } else {
      IncrementPrefValue(prefs::kStabilityRendererCrashCount);

      UMA_HISTOGRAM_SPARSE_SLOWLY("CrashExitCodes.Renderer",
                                  MapCrashExitCodeForHistogram(exit_code));
    }

    UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.ChildCrashes",
                              histogram_type, RENDERER_TYPE_COUNT);
  } else if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED) {
    RecordChildKills(histogram_type);
#if defined(OS_CHROMEOS)
  } else if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM) {
    RecordChildKills(histogram_type);
    UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.ChildKills.OOM",
                              was_extension_process ? 2 : 1,
                              3);
    memory::RecordMemoryStats(
        was_extension_process
            ? memory::RECORD_MEMORY_STATS_EXTENSIONS_OOM_KILLED
            : memory::RECORD_MEMORY_STATS_CONTENTS_OOM_KILLED);
#endif
  } else if (status == base::TERMINATION_STATUS_STILL_RUNNING) {
    UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.DisconnectedAlive",
                              histogram_type, RENDERER_TYPE_COUNT);
  } else if (status == base::TERMINATION_STATUS_LAUNCH_FAILED) {
    UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.ChildLaunchFailures",
                              histogram_type, RENDERER_TYPE_COUNT);
    if (was_extension_process)
      IncrementPrefValue(prefs::kStabilityExtensionRendererFailedLaunchCount);
    else
      IncrementPrefValue(prefs::kStabilityRendererFailedLaunchCount);
  }
}

void ChromeStabilityMetricsProvider::IncrementPrefValue(const char* path) {
  int value = local_state_->GetInteger(path);
  local_state_->SetInteger(path, value + 1);
}

void ChromeStabilityMetricsProvider::IncrementLongPrefsValue(const char* path) {
  int64 value = local_state_->GetInt64(path);
  local_state_->SetInt64(path, value + 1);
}

void ChromeStabilityMetricsProvider::LogRendererHang() {
  IncrementPrefValue(prefs::kStabilityRendererHangCount);
}
