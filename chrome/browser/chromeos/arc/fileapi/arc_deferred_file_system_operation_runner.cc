// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_deferred_file_system_operation_runner.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/arc/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

ArcDeferredFileSystemOperationRunner::ArcDeferredFileSystemOperationRunner(
    ArcBridgeService* bridge_service)
    : ArcDeferredFileSystemOperationRunner(bridge_service, true) {}

ArcDeferredFileSystemOperationRunner::ArcDeferredFileSystemOperationRunner(
    ArcBridgeService* bridge_service,
    bool observe_events)
    : ArcFileSystemOperationRunner(bridge_service),
      observe_events_(observe_events),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (observe_events_) {
    ArcSessionManager::Get()->AddObserver(this);
    arc_bridge_service()->file_system()->AddObserver(this);
    OnStateChanged();
  }
}

ArcDeferredFileSystemOperationRunner::~ArcDeferredFileSystemOperationRunner() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (observe_events_) {
    ArcSessionManager::Get()->RemoveObserver(this);
    arc_bridge_service()->file_system()->RemoveObserver(this);
  }
  // On destruction, deferred operations are discarded.
}

void ArcDeferredFileSystemOperationRunner::GetFileSize(
    const GURL& url,
    const GetFileSizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::Bind(&ArcDeferredFileSystemOperationRunner::GetFileSize,
                   weak_ptr_factory_.GetWeakPtr(), url, callback));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->file_system(), GetFileSize);
  if (!file_system_instance) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, -1));
    return;
  }
  file_system_instance->GetFileSize(url.spec(), callback);
}

void ArcDeferredFileSystemOperationRunner::OpenFileToRead(
    const GURL& url,
    const OpenFileToReadCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::Bind(&ArcDeferredFileSystemOperationRunner::OpenFileToRead,
                   weak_ptr_factory_.GetWeakPtr(), url, callback));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->file_system(), OpenFileToRead);
  if (!file_system_instance) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, base::Passed(mojo::ScopedHandle())));
    return;
  }
  file_system_instance->OpenFileToRead(url.spec(), callback);
}

void ArcDeferredFileSystemOperationRunner::GetDocument(
    const std::string& authority,
    const std::string& document_id,
    const GetDocumentCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (should_defer_) {
    deferred_operations_.emplace_back(base::Bind(
        &ArcDeferredFileSystemOperationRunner::GetDocument,
        weak_ptr_factory_.GetWeakPtr(), authority, document_id, callback));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->file_system(), GetDocument);
  if (!file_system_instance) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, base::Passed(mojom::DocumentPtr())));
    return;
  }
  file_system_instance->GetDocument(authority, document_id, callback);
}

void ArcDeferredFileSystemOperationRunner::GetChildDocuments(
    const std::string& authority,
    const std::string& parent_document_id,
    const GetChildDocumentsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::Bind(&ArcDeferredFileSystemOperationRunner::GetChildDocuments,
                   weak_ptr_factory_.GetWeakPtr(), authority,
                   parent_document_id, callback));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->file_system(), GetChildDocuments);
  if (!file_system_instance) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, base::nullopt));
    return;
  }
  file_system_instance->GetChildDocuments(authority, parent_document_id,
                                          callback);
}

void ArcDeferredFileSystemOperationRunner::OnArcOptInChanged(bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnStateChanged();
}

void ArcDeferredFileSystemOperationRunner::OnInstanceReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnStateChanged();
}

void ArcDeferredFileSystemOperationRunner::OnInstanceClosed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnStateChanged();
}

void ArcDeferredFileSystemOperationRunner::OnStateChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SetShouldDefer(ArcSessionManager::Get()->IsArcEnabled() &&
                 !arc_bridge_service()->file_system()->has_instance());
}

void ArcDeferredFileSystemOperationRunner::SetShouldDefer(bool should_defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  should_defer_ = should_defer;

  if (should_defer_)
    return;

  // Run deferred operations.
  std::vector<base::Closure> deferred_operations;
  deferred_operations.swap(deferred_operations_);
  for (const base::Closure& operation : deferred_operations) {
    operation.Run();
  }

  // No deferred operations should be left at this point.
  DCHECK(deferred_operations_.empty());
}

}  // namespace arc
