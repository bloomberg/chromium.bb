// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/net_http_session_params_observer.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Called on IOThread to disable QUIC for globally-owned HttpNetworkSessions
// and for the profile (thrpugh |disable_quic_callback|). Note that re-enabling
// QUIC dynamically is not supported for simpliciy and requires a browser
// restart.
void DisableQuicOnIOThread(
    NetHttpSessionParamsObserver::DisableQuicCallback disable_quic_callback,
    IOThread* io_thread,
    safe_browsing::SafeBrowsingService* safe_browsing_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Disable QUIC for globally-owned objects.
  io_thread->DisableQuic();
  safe_browsing_service->DisableQuicOnIOThread();

  // Call profile's disable QUIC callback.
  disable_quic_callback.Run();
}

}  // namespace

NetHttpSessionParamsObserver::NetHttpSessionParamsObserver(
    PrefService* prefs,
    DisableQuicCallback disable_quic_callback)
    : disable_quic_callback_(disable_quic_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(prefs);

  base::Closure prefs_callback = base::Bind(
      &NetHttpSessionParamsObserver::ApplySettings, base::Unretained(this));
  quic_allowed_.Init(prefs::kQuicAllowed, prefs, prefs_callback);

  // Apply the initial settings, in case QUIC is disallowed by policy on profile
  // initialization time. Note that even if this may not affect the profile
  // which is being initialized directly (as its URLRequestContext will not be
  // constructed yet), it still applies to globally-owned HttpNetworkSessions
  // and to IOThread's HttpNetworkSession Params, which will in turn be used as
  // a base for the new profile's URLRequestContext.
  ApplySettings();
}

NetHttpSessionParamsObserver::~NetHttpSessionParamsObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void NetHttpSessionParamsObserver::ApplySettings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool quic_allowed_for_profile = true;
  if (quic_allowed_.IsManaged())
    quic_allowed_for_profile = *quic_allowed_;

  // We only support disabling QUIC.
  if (quic_allowed_for_profile)
    return;

  // Disabling QUIC for a profile also disables QUIC for globally-owned
  // HttpNetworkSessions. As a side effect, new Profiles will also have QUIC
  // disabled (because they inherit IOThread's HttpNetworkSession Params).
  IOThread* io_thread = g_browser_process->io_thread();
  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DisableQuicOnIOThread, disable_quic_callback_, io_thread,
                 base::Unretained(safe_browsing_service)));
}

// static
void NetHttpSessionParamsObserver::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kQuicAllowed, true);
}
