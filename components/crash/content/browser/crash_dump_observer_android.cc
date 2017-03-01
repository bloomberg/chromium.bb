// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_dump_observer_android.h"

#include <unistd.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

namespace breakpad {

namespace {
base::LazyInstance<CrashDumpObserver> g_instance = LAZY_INSTANCE_INITIALIZER;
}

// static
void CrashDumpObserver::Create() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If this DCHECK fails in a unit test then a previously executing
  // test that makes use of CrashDumpObserver forgot to create a
  // ShadowingAtExitManager.
  DCHECK(g_instance == nullptr);
  g_instance.Get();
}

// static
CrashDumpObserver* CrashDumpObserver::GetInstance() {
  DCHECK(!(g_instance == nullptr));
  return g_instance.Pointer();
}

CrashDumpObserver::CrashDumpObserver() {
  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                              content::NotificationService::AllSources());
  BrowserChildProcessObserver::Add(this);
}

CrashDumpObserver::~CrashDumpObserver() {
  BrowserChildProcessObserver::Remove(this);
}

void CrashDumpObserver::RegisterClient(std::unique_ptr<Client> client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock auto_lock(registered_clients_lock_);
  registered_clients_.push_back(std::move(client));
}

void CrashDumpObserver::OnChildExit(int child_process_id,
                                    base::ProcessHandle pid,
                                    content::ProcessType process_type,
                                    base::TerminationStatus termination_status,
                                    base::android::ApplicationState app_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<Client*> registered_clients_copy;
  {
    base::AutoLock auto_lock(registered_clients_lock_);
    for (auto& client : registered_clients_)
      registered_clients_copy.push_back(client.get());
  }
  for (auto* client : registered_clients_copy) {
    client->OnChildExit(child_process_id, pid, process_type, termination_status,
                        app_state);
  }
}

void CrashDumpObserver::BrowserChildProcessStarted(
    int child_process_id,
    content::FileDescriptorInfo* mappings) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  std::vector<Client*> registered_clients_copy;
  {
    base::AutoLock auto_lock(registered_clients_lock_);
    for (auto& client : registered_clients_)
      registered_clients_copy.push_back(client.get());
  }
  for (auto* client : registered_clients_copy) {
    client->OnChildStart(child_process_id, mappings);
  }
}

void CrashDumpObserver::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnChildExit(data.id, data.handle,
              static_cast<content::ProcessType>(data.process_type),
              base::TERMINATION_STATUS_MAX_ENUM,
              base::android::ApplicationStatusListener::GetState());
}

void CrashDumpObserver::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    int exit_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnChildExit(data.id, data.handle,
              static_cast<content::ProcessType>(data.process_type),
              base::TERMINATION_STATUS_ABNORMAL_TERMINATION,
              base::android::APPLICATION_STATE_UNKNOWN);
}

void CrashDumpObserver::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderProcessHost* rph =
      content::Source<content::RenderProcessHost>(source).ptr();
  base::TerminationStatus term_status = base::TERMINATION_STATUS_MAX_ENUM;
  base::android::ApplicationState app_state =
      base::android::APPLICATION_STATE_UNKNOWN;
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      // NOTIFICATION_RENDERER_PROCESS_TERMINATED is sent when the renderer
      // process is cleanly shutdown.
      term_status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      // We do not care about android fast shutdowns as it is a known case where
      // the renderer is intentionally killed when we are done with it.
      if (rph->FastShutdownStarted()) {
        break;
      }
      content::RenderProcessHost::RendererClosedDetails* process_details =
          content::Details<content::RenderProcessHost::RendererClosedDetails>(
              details)
              .ptr();
      term_status = process_details->status;
      app_state = base::android::ApplicationStatusListener::GetState();
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      // The child process pid isn't available when process is gone, keep a
      // mapping between child_process_id and pid, so we can find it later.
      child_process_id_to_pid_[rph->GetID()] = rph->GetHandle();
      return;
    }
    default:
      NOTREACHED();
      return;
  }
  base::ProcessHandle pid = rph->GetHandle();
  const auto& iter = child_process_id_to_pid_.find(rph->GetID());
  if (iter != child_process_id_to_pid_.end()) {
    if (pid == base::kNullProcessHandle) {
      pid = iter->second;
    }
    child_process_id_to_pid_.erase(iter);
  }
  OnChildExit(rph->GetID(), pid, content::PROCESS_TYPE_RENDERER, term_status,
              app_state);
}

}  // namespace breakpad
