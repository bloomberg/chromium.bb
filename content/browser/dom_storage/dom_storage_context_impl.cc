// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/message_loop_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_context.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"

using content::BrowserThread;
using content::DOMStorageContext;
using dom_storage::DomStorageArea;
using dom_storage::DomStorageContext;
using dom_storage::DomStorageTaskRunner;
using dom_storage::DomStorageWorkerPoolTaskRunner;

namespace {

const char kLocalStorageDirectory[] = "Local Storage";

void InvokeUsageInfoCallbackHelper(
      const DOMStorageContext::GetUsageInfoCallback& callback,
      const std::vector<DomStorageContext::UsageInfo>* infos) {
  callback.Run(*infos);
}

void GetUsageInfoHelper(
      base::MessageLoopProxy* reply_loop,
      DomStorageContext* context,
      const DOMStorageContext::GetUsageInfoCallback& callback) {
  std::vector<DomStorageContext::UsageInfo>* infos =
      new std::vector<DomStorageContext::UsageInfo>;
  context->GetUsageInfo(infos, true);
  reply_loop->PostTask(
      FROM_HERE,
      base::Bind(&InvokeUsageInfoCallbackHelper,
                 callback, base::Owned(infos)));
}

}  // namespace

DOMStorageContextImpl::DOMStorageContextImpl(
    const FilePath& data_path,
    quota::SpecialStoragePolicy* special_storage_policy) {
  base::SequencedWorkerPool* worker_pool = BrowserThread::GetBlockingPool();
  // TODO(marja): Pass a nonempty session storage directory when session storage
  // is backed on disk.
  context_ = new dom_storage::DomStorageContext(
      data_path.empty() ?
          data_path : data_path.AppendASCII(kLocalStorageDirectory),
      FilePath(),  // Empty session storage directory.
      special_storage_policy,
      new DomStorageWorkerPoolTaskRunner(
          worker_pool,
          worker_pool->GetNamedSequenceToken("dom_storage_primary"),
          worker_pool->GetNamedSequenceToken("dom_storage_commit"),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
}

DOMStorageContextImpl::~DOMStorageContextImpl() {
}

void DOMStorageContextImpl::GetUsageInfo(const GetUsageInfoCallback& callback) {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&GetUsageInfoHelper,
                 base::MessageLoopProxy::current(),
                 context_, callback));
}

void DOMStorageContextImpl::DeleteOrigin(const GURL& origin) {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&DomStorageContext::DeleteOrigin, context_, origin));
}

void DOMStorageContextImpl::PurgeMemory() {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&DomStorageContext::PurgeMemory, context_));
}

void DOMStorageContextImpl::SetForceKeepSessionState() {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&DomStorageContext::SetForceKeepSessionState, context_));
}

void DOMStorageContextImpl::Shutdown() {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&DomStorageContext::Shutdown, context_));
}
