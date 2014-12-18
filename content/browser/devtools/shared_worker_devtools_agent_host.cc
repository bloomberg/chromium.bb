// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_worker_devtools_agent_host.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

void TerminateSharedWorkerOnIO(
    EmbeddedWorkerDevToolsAgentHost::WorkerId worker_id) {
  SharedWorkerServiceImpl::GetInstance()->TerminateWorker(
      worker_id.first, worker_id.second);
}

}  // namespace

SharedWorkerDevToolsAgentHost::SharedWorkerDevToolsAgentHost(
    WorkerId worker_id,
    const SharedWorkerInstance& shared_worker)
    : EmbeddedWorkerDevToolsAgentHost(worker_id),
      shared_worker_(new SharedWorkerInstance(shared_worker)) {
}

DevToolsAgentHost::Type SharedWorkerDevToolsAgentHost::GetType() {
  return TYPE_SHARED_WORKER;
}

std::string SharedWorkerDevToolsAgentHost::GetTitle() {
  return base::UTF16ToUTF8(shared_worker_->name());
}

GURL SharedWorkerDevToolsAgentHost::GetURL() {
  return shared_worker_->url();
}

bool SharedWorkerDevToolsAgentHost::Activate() {
  return false;
}

bool SharedWorkerDevToolsAgentHost::Close() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&TerminateSharedWorkerOnIO, worker_id()));
  return true;
}

bool SharedWorkerDevToolsAgentHost::Matches(
    const SharedWorkerInstance& other) {
  return shared_worker_->Matches(other);
}

SharedWorkerDevToolsAgentHost::~SharedWorkerDevToolsAgentHost() {
}

}  // namespace content
