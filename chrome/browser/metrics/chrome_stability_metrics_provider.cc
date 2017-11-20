// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/features/features.h"
#include "ppapi/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_map.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/metrics/plugin_metrics_provider.h"
#endif

ChromeStabilityMetricsProvider::ChromeStabilityMetricsProvider(
    PrefService* local_state)
    :
#if defined(OS_ANDROID)
      scoped_observer_(this),
#endif  // defined(OS_ANDROID)
      helper_(local_state) {
  BrowserChildProcessObserver::Add(this);

#if defined(OS_ANDROID)
  auto* crash_manager = breakpad::CrashDumpManager::GetInstance();
  DCHECK(crash_manager);
  scoped_observer_.Add(crash_manager);
#endif  // defined(OS_ANDROID)
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
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
}

void ChromeStabilityMetricsProvider::OnRecordingDisabled() {
  registrar_.RemoveAll();
}

void ChromeStabilityMetricsProvider::ProvideStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  helper_.ProvideStabilityMetrics(system_profile_proto);
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
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

    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      bool was_extension_process = false;
#if BUILDFLAG(ENABLE_EXTENSIONS)
      content::RenderProcessHost* host =
          content::Source<content::RenderProcessHost>(source).ptr();
      if (extensions::ProcessMap::Get(host->GetBrowserContext())
              ->Contains(host->GetID())) {
        was_extension_process = true;
      }
#endif
      helper_.LogRendererLaunched(was_extension_process);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void ChromeStabilityMetricsProvider::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    int exit_code) {
#if BUILDFLAG(ENABLE_PLUGINS)
  // Exclude plugin crashes from the count below because we report them via
  // a separate UMA metric.
  if (PluginMetricsProvider::IsPluginProcess(data.process_type))
    return;
#endif

  helper_.BrowserChildProcessCrashed();
}

#if defined(OS_ANDROID)
void ChromeStabilityMetricsProvider::OnCrashDumpProcessed(
    const breakpad::CrashDumpManager::CrashDumpDetails& details) {
  // There is a delay for OOM flag to be removed when app goes to background, so
  // we can't just check for OOM_PROTECTED flag.
  if (details.status ==
          breakpad::CrashDumpManager::CrashDumpStatus::kValidDump &&
      details.process_type == content::PROCESS_TYPE_RENDERER &&
      details.termination_status == base::TERMINATION_STATUS_OOM_PROTECTED &&
      (details.app_state ==
           base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES ||
       details.app_state ==
           base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES)) {
    helper_.IncreaseRendererCrashCount();
  }
}
#endif  // defined(OS_ANDROID)
