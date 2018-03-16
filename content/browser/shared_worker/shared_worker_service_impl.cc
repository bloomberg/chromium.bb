// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_service_impl.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/shared_worker/shared_worker_client.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/bind_interface_helpers.h"
#include "third_party/WebKit/public/common/message_port/message_port_channel.h"
#include "url/origin.h"

namespace content {
namespace {

bool IsShuttingDown(RenderProcessHost* host) {
  return !host || host->FastShutdownStarted() ||
         host->IsKeepAliveRefCountDisabled();
}

}  // namespace

SharedWorkerServiceImpl::SharedWorkerServiceImpl() {}

SharedWorkerServiceImpl::~SharedWorkerServiceImpl() {}

bool SharedWorkerServiceImpl::TerminateWorker(
    const GURL& url,
    const std::string& name,
    const url::Origin& constructor_origin) {
  for (auto& host : worker_hosts_) {
    if (host->IsAvailable() &&
        host->instance()->Matches(url, name, constructor_origin)) {
      host->TerminateWorker();
      return true;
    }
  }
  return false;
}

void SharedWorkerServiceImpl::TerminateAllWorkersForTesting(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!terminate_all_workers_callback_);
  if (worker_hosts_.empty()) {
    // Run callback asynchronously to avoid re-entering the caller.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  } else {
    terminate_all_workers_callback_ = std::move(callback);
    for (auto& host : worker_hosts_)
      host->TerminateWorker();
    // Monitor for actual termination in DestroyHost.
  }
}

void SharedWorkerServiceImpl::ConnectToWorker(
    int process_id,
    int frame_id,
    mojom::SharedWorkerInfoPtr info,
    mojom::SharedWorkerClientPtr client,
    blink::mojom::SharedWorkerCreationContextType creation_context_type,
    const blink::MessagePortChannel& message_port) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(process_id, frame_id);
  if (!render_frame_host) {
    // TODO(nhiroki): Support the case where the requester is a worker (i.e.,
    // nested worker) (https://crbug.com/31666).
    client->OnScriptLoadFailed();
    return;
  }

  RenderFrameHost* main_frame =
      render_frame_host->frame_tree_node()->frame_tree()->GetMainFrame();
  if (!GetContentClient()->browser()->AllowSharedWorker(
          info->url, main_frame->GetLastCommittedURL(), info->name,
          render_frame_host->GetLastCommittedOrigin(),
          WebContentsImpl::FromRenderFrameHostID(process_id, frame_id)
              ->GetBrowserContext(),
          process_id, frame_id)) {
    client->OnScriptLoadFailed();
    return;
  }

  auto instance = std::make_unique<SharedWorkerInstance>(
      info->url, info->name, render_frame_host->GetLastCommittedOrigin(),
      info->content_security_policy, info->content_security_policy_type,
      info->creation_address_space, creation_context_type);

  SharedWorkerHost* host = FindAvailableSharedWorkerHost(*instance);
  if (host) {
    // Non-secure contexts cannot connect to secure workers, and secure contexts
    // cannot connect to non-secure workers:
    if (host->instance()->creation_context_type() != creation_context_type) {
      client->OnScriptLoadFailed();
      return;
    }

    // The process may be shutting down, in which case we will try to create a
    // new shared worker instead.
    if (!IsShuttingDown(RenderProcessHost::FromID(host->process_id()))) {
      host->AddClient(std::move(client), process_id, frame_id, message_port);
      return;
    }
    // Cleanup the existing shared worker now, to avoid having two matching
    // instances. This host would likely be observing the destruction of the
    // child process shortly, but we can clean this up now to avoid some
    // complexity.
    DestroyHost(host);
  }

  CreateWorker(std::move(instance), std::move(client), process_id, frame_id,
               message_port);
}

void SharedWorkerServiceImpl::DestroyHost(SharedWorkerHost* host) {
  RenderProcessHost* process_host =
      RenderProcessHost::FromID(host->process_id());
  worker_hosts_.erase(worker_hosts_.find(host));

  // Complete the call to TerminateAllWorkersForTesting if no more workers.
  if (worker_hosts_.empty() && terminate_all_workers_callback_)
    std::move(terminate_all_workers_callback_).Run();

  if (!IsShuttingDown(process_host))
    process_host->DecrementKeepAliveRefCount();
}

void SharedWorkerServiceImpl::CreateWorker(
    std::unique_ptr<SharedWorkerInstance> instance,
    mojom::SharedWorkerClientPtr client,
    int process_id,
    int frame_id,
    const blink::MessagePortChannel& message_port) {
  // Re-use the process that requested the shared worker.
  int worker_process_id = process_id;

  RenderProcessHost* process_host = RenderProcessHost::FromID(process_id);
  // If the requesting process is shutting down, then just drop this request on
  // the floor. The client is not going to be around much longer anyways.
  if (IsShuttingDown(process_host))
    return;

  // Keep the renderer process alive that will be hosting the shared worker.
  process_host->IncrementKeepAliveRefCount();

  auto host = std::make_unique<SharedWorkerHost>(this, std::move(instance),
                                                 worker_process_id);

  bool pause_on_start;
  base::UnguessableToken devtools_worker_token;
  SharedWorkerDevToolsManager::GetInstance()->WorkerCreated(
      host.get(), &pause_on_start, &devtools_worker_token);

  // Get the factory used to instantiate the new shared worker instance in
  // the target process.
  mojom::SharedWorkerFactoryPtr factory;
  BindInterface(process_host, &factory);

  host->Start(std::move(factory), pause_on_start, devtools_worker_token);
  host->AddClient(std::move(client), process_id, frame_id, message_port);

  worker_hosts_.insert(std::move(host));
}

SharedWorkerHost* SharedWorkerServiceImpl::FindAvailableSharedWorkerHost(
    const SharedWorkerInstance& instance) {
  for (auto& host : worker_hosts_) {
    if (host->IsAvailable() && host->instance()->Matches(instance))
      return host.get();
  }
  return nullptr;
}

}  // namespace content
