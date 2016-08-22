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

// static
CrashDumpObserver* CrashDumpObserver::GetInstance() {
  return base::Singleton<CrashDumpObserver>::get();
}

CrashDumpObserver::CrashDumpObserver() {
  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                              content::NotificationService::AllSources());
}

CrashDumpObserver::~CrashDumpObserver() {}

void CrashDumpObserver::OnChildExitOnBlockingPool(
    Client* client,
    int child_process_id,
    base::ProcessHandle pid,
    content::ProcessType process_type,
    base::TerminationStatus termination_status,
    base::android::ApplicationState app_state) {
  base::AutoLock auto_lock(registered_clients_lock_);
  // Only call Client::OnChildExit if we haven't been removed in the
  // interim.
  if (base::ContainsValue(registered_clients_, client)) {
    client->OnChildExit(child_process_id, pid, process_type, termination_status,
                        app_state);
  }
}

void CrashDumpObserver::OnChildExit(int child_process_id,
                                    base::ProcessHandle pid,
                                    content::ProcessType process_type,
                                    base::TerminationStatus termination_status,
                                    base::android::ApplicationState app_state) {
  base::AutoLock auto_lock(registered_clients_lock_);
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken token = pool->GetSequenceToken();

  for (auto& client : registered_clients_) {
    pool->PostSequencedWorkerTask(
        token, FROM_HERE,
        base::Bind(&CrashDumpObserver::OnChildExitOnBlockingPool,
                   base::Unretained(this), client, child_process_id, pid,
                   process_type, termination_status, app_state));
  }
}

void CrashDumpObserver::RegisterClient(Client* client) {
  base::AutoLock auto_lock(registered_clients_lock_);
  if (std::find(std::begin(registered_clients_), std::end(registered_clients_),
                client) != std::end(registered_clients_)) {
    return;
  }
  registered_clients_.push_back(client);
}

void CrashDumpObserver::UnregisterClient(Client* client) {
  base::AutoLock auto_lock(registered_clients_lock_);
  registered_clients_.remove(client);
}

void CrashDumpObserver::BrowserChildProcessStarted(
    int child_process_id,
    content::FileDescriptorInfo* mappings) {
  base::AutoLock auto_lock(registered_clients_lock_);
  for (auto& client : registered_clients_) {
    client->OnChildStart(child_process_id, mappings);
  }
}

void CrashDumpObserver::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  OnChildExit(data.id, data.handle,
              static_cast<content::ProcessType>(data.process_type),
              base::TERMINATION_STATUS_MAX_ENUM,
              base::android::APPLICATION_STATE_UNKNOWN);
}

void CrashDumpObserver::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    int exit_code) {
  OnChildExit(data.id, data.handle,
              static_cast<content::ProcessType>(data.process_type),
              base::TERMINATION_STATUS_ABNORMAL_TERMINATION,
              base::android::APPLICATION_STATE_UNKNOWN);
}

void CrashDumpObserver::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  content::RenderProcessHost* rph =
      content::Source<content::RenderProcessHost>(source).ptr();
  base::TerminationStatus term_status = base::TERMINATION_STATUS_MAX_ENUM;
  base::android::ApplicationState app_state =
      base::android::APPLICATION_STATE_UNKNOWN;
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      // NOTIFICATION_RENDERER_PROCESS_TERMINATED is sent when the renderer
      // process is cleanly shutdown. However, we still need to close the
      // minidump_fd we kept open.
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
    default:
      NOTREACHED();
      return;
  }

  OnChildExit(rph->GetID(), rph->GetHandle(), content::PROCESS_TYPE_RENDERER,
              term_status, app_state);
}

}  // namespace breakpad
