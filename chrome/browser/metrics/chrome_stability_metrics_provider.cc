// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/process_map.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/metrics/plugin_metrics_provider.h"
#endif

#if defined(OS_WIN)
#include <windows.h>  // Needed for STATUS_* codes
#include "chrome/common/metrics_constants_util_win.h"
#include "components/browser_watcher/crash_reporting_metrics_win.h"
#endif

namespace {

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
          chrome::GetBrowserCrashDumpAttemptsRegistryPath())
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

}  // namespace

ChromeStabilityMetricsProvider::ChromeStabilityMetricsProvider(
    PrefService* local_state)
    : helper_(local_state) {
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
  helper_.ProvideStabilityMetrics(system_profile_proto);

#if defined(OS_WIN)
  CountBrowserCrashDumpAttempts();
#endif  // defined(OS_WIN)
}

void ChromeStabilityMetricsProvider::ClearSavedStabilityMetrics() {
  helper_.ClearSavedStabilityMetrics();
}

void ChromeStabilityMetricsProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_START: {
      helper_.LogLoadStarted();
      break;
    }

    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      content::RenderProcessHost::RendererClosedDetails* process_details =
          content::Details<content::RenderProcessHost::RendererClosedDetails>(
              details).ptr();
      bool was_extension_process = false;
#if defined(ENABLE_EXTENSIONS)
      content::RenderProcessHost* host =
          content::Source<content::RenderProcessHost>(source).ptr();
      if (extensions::ProcessMap::Get(host->GetBrowserContext())
              ->Contains(host->GetID())) {
        was_extension_process = true;
      }
#endif
      helper_.LogRendererCrash(was_extension_process, process_details->status,
                               process_details->exit_code);
      break;
    }

    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      helper_.LogRendererHang();
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

  helper_.BrowserChildProcessCrashed();
}
