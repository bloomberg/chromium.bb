// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/database/database_util.h"
#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_context.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"
#include "webkit/glue/webkit_glue.h"

using content::BrowserThread;
using content::DOMStorageContext;
using dom_storage::DomStorageArea;
using dom_storage::DomStorageContext;
using dom_storage::DomStorageTaskRunner;
using dom_storage::DomStorageWorkerPoolTaskRunner;
using webkit_database::DatabaseUtil;

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

// TODO(michaeln): Fix the content layer api, FilePaths and
// string16 origin_ids are just wrong. Then get rid of
// this conversion non-sense. Most of the includes are just
// to support that non-sense.

GURL OriginIdToGURL(const string16& origin_id) {
  return DatabaseUtil::GetOriginFromIdentifier(origin_id);
}

FilePath OriginToFullFilePath(const FilePath& directory,
                              const GURL& origin) {
  return directory.Append(DomStorageArea::DatabaseFileNameFromOrigin(origin));
}

GURL FilePathToOrigin(const FilePath& path) {
  DCHECK(path.MatchesExtension(DomStorageArea::kDatabaseFileExtension));
  return DomStorageArea::OriginFromDatabaseFileName(path);
}

void InvokeAllStorageFilesCallbackHelper(
      const DOMStorageContext::GetAllStorageFilesCallback& callback,
      const std::vector<FilePath>& file_paths) {
  callback.Run(file_paths);
}

void GetAllStorageFilesHelper(
      base::MessageLoopProxy* reply_loop,
      DomStorageContext* context,
      const DOMStorageContext::GetAllStorageFilesCallback& callback) {
  std::vector<DomStorageContext::UsageInfo> infos;
  // TODO(michaeln): Actually include the file info too when the
  // content layer api is fixed.
  const bool kDontIncludeFileInfo = false;
  context->GetUsageInfo(&infos, kDontIncludeFileInfo);

  std::vector<FilePath> paths;
  for (size_t i = 0; i < infos.size(); ++i) {
    paths.push_back(
        OriginToFullFilePath(context->localstorage_directory(),
                             infos[i].origin));
  }

  reply_loop->PostTask(
      FROM_HERE,
      base::Bind(&InvokeAllStorageFilesCallbackHelper,
                 callback, paths));
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

void DOMStorageContextImpl::GetAllStorageFiles(
      const GetAllStorageFilesCallback& callback) {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&GetAllStorageFilesHelper,
                 base::MessageLoopProxy::current(),
                 context_, callback));
}

FilePath DOMStorageContextImpl::GetFilePath(const string16& origin_id) const {
  DCHECK(context_);
  return OriginToFullFilePath(context_->localstorage_directory(),
                              OriginIdToGURL(origin_id));
}

void DOMStorageContextImpl::DeleteForOrigin(const string16& origin_id) {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&DomStorageContext::DeleteOrigin, context_,
                 OriginIdToGURL(origin_id)));
}

void DOMStorageContextImpl::DeleteLocalStorageFile(const FilePath& file_path) {
  DCHECK(context_);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&DomStorageContext::DeleteOrigin, context_,
                 FilePathToOrigin(file_path)));
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

int64 DOMStorageContextImpl::LeakyCloneSessionStorage(
    int64 existing_namespace_id) {
  DCHECK(context_);
  int64 clone_id = context_->AllocateSessionId();
  context_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DomStorageContext::CloneSessionNamespace, context_,
                 existing_namespace_id, clone_id));
  return clone_id;
}
