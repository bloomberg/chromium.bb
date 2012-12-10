// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/process_utils.h"

#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "jni/ProcessUtils_jni.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

void CloseIdleConnectionsForProfile(
    scoped_refptr<net::URLRequestContextGetter> context_getter) {
  DCHECK(context_getter.get());
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                           base::Bind(&CloseIdleConnectionsForProfile,
                                      context_getter));
    return;
  }

  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  if (!context)
    return;
  net::HttpNetworkSession* session =
      context->http_transaction_factory()->GetSession();
  if (session)
    session->CloseIdleConnections();
}

// Only accessed from the JNI thread by ToggleWebKitSharedTimers() which is
// implemented below.
base::LazyInstance<std::vector<int /* process id */> > g_suspended_processes =
    LAZY_INSTANCE_INITIALIZER;

// Suspends timers in all current render processes.
void SuspendWebKitSharedTimers(std::vector<int>* suspended_processes) {
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* host = i.GetCurrentValue();
    suspended_processes->push_back(host->GetID());
    host->Send(new ChromeViewMsg_ToggleWebKitSharedTimer(true));
  }
}

void ResumeWebkitSharedTimers(const std::vector<int>& suspended_processes) {
  for (std::vector<int>::const_iterator it = suspended_processes.begin();
       it != suspended_processes.end(); ++it) {
    content::RenderProcessHost* host = content::RenderProcessHost::FromID(*it);
    if (host)
      host->Send(new ChromeViewMsg_ToggleWebKitSharedTimer(false));
  }
}

}  // namespace

static void ToggleWebKitSharedTimers(JNIEnv* env,
                                     jclass obj,
                                     jboolean suspend) {
  std::vector<int>* suspended_processes = &g_suspended_processes.Get();
  if (suspend) {
    DCHECK(suspended_processes->empty());
    SuspendWebKitSharedTimers(suspended_processes);
  } else {
    ResumeWebkitSharedTimers(*suspended_processes);
    suspended_processes->clear();
  }
}

static void CloseIdleConnections(JNIEnv* env, jclass obj) {
  // Iterate through all loaded profiles (and their associated incognito
  // profiles if created), and close the idle connections associated with each
  // one.
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());
  for (std::vector<Profile*>::iterator i = profiles.begin();
      i != profiles.end(); i++) {
    Profile* profile = *i;
    CloseIdleConnectionsForProfile(profile->GetRequestContext());
    if (profile->HasOffTheRecordProfile()) {
      CloseIdleConnectionsForProfile(
          profile->GetOffTheRecordProfile()->GetRequestContext());
    }
  }
}

bool RegisterProcessUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
