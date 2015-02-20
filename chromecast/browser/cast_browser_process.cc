// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_process.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_resource_dispatcher_host_delegate.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "chromecast/browser/metrics/cast_metrics_service_client.h"
#include "chromecast/browser/service/cast_service.h"
#include "chromecast/net/connectivity_checker.h"

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
  if (pref_service_)
    pref_service_->CommitPendingWrite();
#if defined(USE_AURA)
  aura::Env::DeleteInstance();
#endif
  g_instance = NULL;
}

void CastBrowserProcess::SetBrowserContext(
    scoped_ptr<CastBrowserContext> browser_context) {
  DCHECK(!browser_context_);
  browser_context_.swap(browser_context);
}

void CastBrowserProcess::SetCastService(scoped_ptr<CastService> cast_service) {
  DCHECK(!cast_service_);
  cast_service_.swap(cast_service);
}

void CastBrowserProcess::SetMetricsHelper(
    scoped_ptr<metrics::CastMetricsHelper> metrics_helper) {
  DCHECK(!metrics_helper_);
  metrics_helper_.swap(metrics_helper);
}

void CastBrowserProcess::SetMetricsServiceClient(
    scoped_ptr<metrics::CastMetricsServiceClient> metrics_service_client) {
  DCHECK(!metrics_service_client_);
  metrics_service_client_.swap(metrics_service_client);
}

void CastBrowserProcess::SetPrefService(scoped_ptr<PrefService> pref_service) {
  DCHECK(!pref_service_);
  pref_service_.swap(pref_service);
}

void CastBrowserProcess::SetRemoteDebuggingServer(
    scoped_ptr<RemoteDebuggingServer> remote_debugging_server) {
  DCHECK(!remote_debugging_server_);
  remote_debugging_server_.swap(remote_debugging_server);
}

void CastBrowserProcess::SetResourceDispatcherHostDelegate(
    scoped_ptr<CastResourceDispatcherHostDelegate> delegate) {
  DCHECK(!resource_dispatcher_host_delegate_);
  resource_dispatcher_host_delegate_.swap(delegate);
}

#if defined(OS_ANDROID)
void CastBrowserProcess::SetCrashDumpManager(
    scoped_ptr<breakpad::CrashDumpManager> crash_dump_manager) {
  DCHECK(!crash_dump_manager_);
  crash_dump_manager_.swap(crash_dump_manager);
}
#endif  // defined(OS_ANDROID)

void CastBrowserProcess::SetConnectivityChecker(
    scoped_refptr<ConnectivityChecker> connectivity_checker) {
  DCHECK(!connectivity_checker_);
  connectivity_checker_.swap(connectivity_checker);
}

}  // namespace shell
}  // namespace chromecast
