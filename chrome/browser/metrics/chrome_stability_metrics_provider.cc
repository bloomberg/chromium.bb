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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
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
#endif

namespace {

void IncrementPrefValue(const char* path) {
  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  int value = pref->GetInteger(path);
  pref->SetInteger(path, value + 1);
}

void IncrementLongPrefsValue(const char* path) {
  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  int64 value = pref->GetInt64(path);
  pref->SetInt64(path, value + 1);
}

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

}  // namespace

ChromeStabilityMetricsProvider::ChromeStabilityMetricsProvider() {
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
  PrefService* pref = g_browser_process->local_state();
  metrics::SystemProfileProto_Stability* stability_proto =
      system_profile_proto->mutable_stability();

  int count = pref->GetInteger(prefs::kStabilityPageLoadCount);
  if (count) {
    stability_proto->set_page_load_count(count);
    pref->SetInteger(prefs::kStabilityPageLoadCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityChildProcessCrashCount);
  if (count) {
    stability_proto->set_child_process_crash_count(count);
    pref->SetInteger(prefs::kStabilityChildProcessCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityRendererCrashCount);
  if (count) {
    stability_proto->set_renderer_crash_count(count);
    pref->SetInteger(prefs::kStabilityRendererCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityExtensionRendererCrashCount);
  if (count) {
    stability_proto->set_extension_renderer_crash_count(count);
    pref->SetInteger(prefs::kStabilityExtensionRendererCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityRendererHangCount);
  if (count) {
    stability_proto->set_renderer_hang_count(count);
    pref->SetInteger(prefs::kStabilityRendererHangCount, 0);
  }
}

void ChromeStabilityMetricsProvider::ClearSavedStabilityMetrics() {
  PrefService* local_state = g_browser_process->local_state();

  // Clear all the prefs used in this class in UMA reports (which doesn't
  // include |kUninstallMetricsPageLoadCount| as it's not sent up by UMA).
  local_state->SetInteger(prefs::kStabilityChildProcessCrashCount, 0);
  local_state->SetInteger(prefs::kStabilityExtensionRendererCrashCount, 0);
  local_state->SetInteger(prefs::kStabilityPageLoadCount, 0);
  local_state->SetInteger(prefs::kStabilityRendererCrashCount, 0);
  local_state->SetInteger(prefs::kStabilityRendererHangCount, 0);
}

// static
void ChromeStabilityMetricsProvider::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kStabilityChildProcessCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityExtensionRendererCrashCount,
                                0);
  registry->RegisterIntegerPref(prefs::kStabilityPageLoadCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);
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
    const content::ChildProcessData& data) {
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
  bool was_extension_process = false;
#if defined(ENABLE_EXTENSIONS)
  was_extension_process =
      extensions::ProcessMap::Get(host->GetBrowserContext())->Contains(
          host->GetID());
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

    UMA_HISTOGRAM_PERCENTAGE("BrowserRenderProcessHost.ChildCrashes",
                             was_extension_process ? 2 : 1);
  } else if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED) {
    UMA_HISTOGRAM_PERCENTAGE("BrowserRenderProcessHost.ChildKills",
                             was_extension_process ? 2 : 1);
  } else if (status == base::TERMINATION_STATUS_STILL_RUNNING) {
    UMA_HISTOGRAM_PERCENTAGE("BrowserRenderProcessHost.DisconnectedAlive",
                             was_extension_process ? 2 : 1);
  }
}

void ChromeStabilityMetricsProvider::LogRendererHang() {
  IncrementPrefValue(prefs::kStabilityRendererHangCount);
}
