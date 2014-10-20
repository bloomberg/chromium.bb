// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_process.h"

#include "base/logging.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "chromecast/browser/metrics/cast_metrics_service_client.h"
#include "chromecast/browser/service/cast_service.h"

#if defined(OS_ANDROID)
#include "components/crash/browser/crash_dump_manager_android.h"
#endif  // defined(OS_ANDROID)

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace chromecast {
namespace shell {

namespace {
CastBrowserProcess* g_instance = NULL;
}  // namespace

// static
CastBrowserProcess* CastBrowserProcess::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

CastBrowserProcess::CastBrowserProcess() {
  DCHECK(!g_instance);
  g_instance = this;
}

CastBrowserProcess::~CastBrowserProcess() {
  DCHECK_EQ(g_instance, this);
#if defined(USE_AURA)
  aura::Env::DeleteInstance();
#endif
  g_instance = NULL;
}

void CastBrowserProcess::SetBrowserContext(
    CastBrowserContext* browser_context) {
  DCHECK(!browser_context_);
  browser_context_.reset(browser_context);
}

void CastBrowserProcess::SetCastService(CastService* cast_service) {
  DCHECK(!cast_service_);
  cast_service_.reset(cast_service);
}

void CastBrowserProcess::SetRemoteDebuggingServer(
    RemoteDebuggingServer* remote_debugging_server) {
  DCHECK(!remote_debugging_server_);
  remote_debugging_server_.reset(remote_debugging_server);
}

void CastBrowserProcess::SetMetricsHelper(
    metrics::CastMetricsHelper* metrics_helper) {
  DCHECK(!metrics_helper_);
  metrics_helper_.reset(metrics_helper);
}

void CastBrowserProcess::SetMetricsServiceClient(
    metrics::CastMetricsServiceClient* metrics_service_client) {
  DCHECK(!metrics_service_client_);
  metrics_service_client_.reset(metrics_service_client);
}

#if defined(OS_ANDROID)
void CastBrowserProcess::SetCrashDumpManager(
    breakpad::CrashDumpManager* crash_dump_manager) {
  DCHECK(!crash_dump_manager_);
  crash_dump_manager_.reset(crash_dump_manager);
}
#endif  // defined(OS_ANDROID)

}  // namespace shell
}  // namespace chromecast
