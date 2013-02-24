// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/message_loop_proxy.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_context.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"

using dom_storage::DomStorageArea;
using dom_storage::DomStorageContext;
using dom_storage::DomStorageTaskRunner;
using dom_storage::DomStorageWorkerPoolTaskRunner;

namespace content {
namespace {

const char kLocalStorageDirectory[] = "Local Storage";
const char kSessionStorageDirectory[] = "Session Storage";

void InvokeLocalStorageUsageCallbackHelper(
      const DOMStorageContext::GetLocalStorageUsageCallback& callback,
      const std::vector<dom_storage::LocalStorageUsageInfo>* infos) {
  callback.Run(*infos);
}

void GetLocalStorageUsageHelper(
    base::MessageLoopProxy* reply_loop,
    DomStorageContext* context,
    const DOMStorageContext::GetLocalStorageUsageCallback& callback) {
  std::vector<dom_storage::LocalStorageUsageInfo>* infos =
      new std::vector<dom_storage::LocalStorageUsageInfo>;
  context->GetLocalStorageUsage(infos, true);
  reply_loop->PostTask(
      FROM_HERE,
      base::Bind(&InvokeLocalStorageUsageCallbackHelper,
                 callback, base::Owned(infos)));
}

void InvokeSessionStorageUsageCallbackHelper(
      const DOMStorageContext::GetSessionStorageUsageCallback& callback,
      const std::vector<dom_storage::SessionStorageUsageInfo>* infos) {
  callback.Run(*infos);
}

void GetSessionStorageUsageHelper(
    base::MessageLoopProxy* reply_loop,
    DomStorageContext* context,
    const DOMStorageContext::GetSessionStorageUsageCallback& callback) {
  std::vector<dom_storage::SessionStorageUsageInfo>* infos =
      new std::vector<dom_storage::SessionStorageUsageInfo>;
  context->GetSessionStorageUsage(infos);
  reply_loop->PostTask(
      FROM_HERE,
      base::Bind(&InvokeSessionStorageUsageCallbackHelper,
                 callback, base::Owned(infos)));
}

}  // namespace

DOMStorageContextImpl::DOMStorageContextImpl(
    const base::FilePath& data_path,
    quota::SpecialStoragePolicy* special_storage_policy) {
  base::SequencedWorkerPool* worker_pool = BrowserThread::GetBlockingPool();
  context_ = new dom_storage::DomStorageContext(
      data_path.empty() ?
          data_path : data_path.AppendASCII(kLocalStorageDirectory),
      data_path.empty() ?
          data_path : data_path.AppendASCII(kSessionStorageDirectory),
      special_storage_policy,
      new DomStorageWorkerPoolTaskRunner(
          worker_pool,
          worker_pool->GetNamedSequenceToken("dom_storage_primary"),
          worker_pool->GetNamedSequenceToken("dom_storage_commit"),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
}

DOMStorageContextImpl::~DOMStorageContextImpl() {
}

void DOMStorageContextImpl::GetLocalStorageUsage(
    const GetLocalStorageUsageCallback& callback) {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&GetLocalStorageUsageHelper,
                 base::MessageLoopProxy::current(),
                 context_, callback));
}

void DOMStorageContextImpl::GetSessionStorageUsage(
    const GetSessionStorageUsageCallback& callback) {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&GetSessionStorageUsageHelper,
                 base::MessageLoopProxy::current(),
                 context_, callback));
}

void DOMStorageContextImpl::DeleteLocalStorage(const GURL& origin) {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&DomStorageContext::DeleteLocalStorage, context_, origin));
}

void DOMStorageContextImpl::DeleteSessionStorage(
    const dom_storage::SessionStorageUsageInfo& usage_info) {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&DomStorageContext::DeleteSessionStorage, context_,
                 usage_info));
}

void DOMStorageContextImpl::SetSaveSessionStorageOnDisk() {
  DCHECK(context_);
  context_->SetSaveSessionStorageOnDisk();
}

scoped_refptr<SessionStorageNamespace>
DOMStorageContextImpl::RecreateSessionStorage(
    const std::string& persistent_id) {
  return scoped_refptr<SessionStorageNamespace>(
      new SessionStorageNamespaceImpl(this, persistent_id));
}

void DOMStorageContextImpl::StartScavengingUnusedSessionStorage() {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&DomStorageContext::StartScavengingUnusedSessionStorage,
                 context_));
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

}  // namespace content
