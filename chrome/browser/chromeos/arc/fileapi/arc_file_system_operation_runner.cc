// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "components/arc/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

namespace {

// TODO(nya): Use typemaps.
ArcFileSystemOperationRunner::ChangeType FromMojoChangeType(
    mojom::ChangeType type) {
  switch (type) {
    case mojom::ChangeType::CHANGED:
      return ArcFileSystemOperationRunner::ChangeType::CHANGED;
    case mojom::ChangeType::DELETED:
      return ArcFileSystemOperationRunner::ChangeType::DELETED;
  }
  NOTREACHED();
  return ArcFileSystemOperationRunner::ChangeType::CHANGED;
}

}  // namespace

// static
const char ArcFileSystemOperationRunner::kArcServiceName[] =
    "arc::ArcFileSystemOperationRunner";

// static
std::unique_ptr<ArcFileSystemOperationRunner>
ArcFileSystemOperationRunner::CreateForTesting(
    ArcBridgeService* bridge_service) {
  // We can't use base::MakeUnique() here because we are calling a private
  // constructor.
  return base::WrapUnique<ArcFileSystemOperationRunner>(
      new ArcFileSystemOperationRunner(bridge_service, nullptr, false));
}

ArcFileSystemOperationRunner::ArcFileSystemOperationRunner(
    ArcBridgeService* bridge_service,
    const Profile* profile)
    : ArcFileSystemOperationRunner(bridge_service, profile, true) {
  DCHECK(profile);
}

ArcFileSystemOperationRunner::ArcFileSystemOperationRunner(
    ArcBridgeService* bridge_service,
    const Profile* profile,
    bool set_should_defer_by_events)
    : ArcService(bridge_service),
      profile_(profile),
      set_should_defer_by_events_(set_should_defer_by_events),
      binding_(this),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We need to observe FileSystemInstance even in unit tests to call Init().
  arc_bridge_service()->file_system()->AddObserver(this);

  // ArcSessionManager may not exist in unit tests.
  auto* arc_session_manager = ArcSessionManager::Get();
  if (arc_session_manager)
    arc_session_manager->AddObserver(this);

  OnStateChanged();
}

ArcFileSystemOperationRunner::~ArcFileSystemOperationRunner() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* arc_session_manager = ArcSessionManager::Get();
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);

  arc_bridge_service()->file_system()->RemoveObserver(this);
  // On destruction, deferred operations are discarded.
}

void ArcFileSystemOperationRunner::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void ArcFileSystemOperationRunner::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

void ArcFileSystemOperationRunner::GetFileSize(
    const GURL& url,
    const GetFileSizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::Bind(&ArcFileSystemOperationRunner::GetFileSize,
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

void ArcFileSystemOperationRunner::OpenFileToRead(
    const GURL& url,
    const OpenFileToReadCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::Bind(&ArcFileSystemOperationRunner::OpenFileToRead,
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

void ArcFileSystemOperationRunner::GetDocument(
    const std::string& authority,
    const std::string& document_id,
    const GetDocumentCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (should_defer_) {
    deferred_operations_.emplace_back(base::Bind(
        &ArcFileSystemOperationRunner::GetDocument,
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

void ArcFileSystemOperationRunner::GetChildDocuments(
    const std::string& authority,
    const std::string& parent_document_id,
    const GetChildDocumentsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::Bind(&ArcFileSystemOperationRunner::GetChildDocuments,
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

void ArcFileSystemOperationRunner::AddWatcher(
    const std::string& authority,
    const std::string& document_id,
    const WatcherCallback& watcher_callback,
    const AddWatcherCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (should_defer_) {
    deferred_operations_.emplace_back(
        base::Bind(&ArcFileSystemOperationRunner::AddWatcher,
                   weak_ptr_factory_.GetWeakPtr(), authority, document_id,
                   watcher_callback, callback));
    return;
  }
  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->file_system(), AddWatcher);
  if (!file_system_instance) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, -1));
    return;
  }
  file_system_instance->AddWatcher(
      authority, document_id,
      base::Bind(&ArcFileSystemOperationRunner::OnWatcherAdded,
                 weak_ptr_factory_.GetWeakPtr(), watcher_callback, callback));
}

void ArcFileSystemOperationRunner::RemoveWatcher(
    int64_t watcher_id,
    const RemoveWatcherCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // RemoveWatcher() is never deferred since watchers do not persist across
  // container reboots.
  if (should_defer_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }

  // Unregister from |watcher_callbacks_| now because we will do it even if
  // the remote method fails anyway. This is an implementation detail, so
  // users must not assume registered callbacks are immediately invalidated.
  auto iter = watcher_callbacks_.find(watcher_id);
  if (iter == watcher_callbacks_.end()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }
  watcher_callbacks_.erase(iter);

  auto* file_system_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->file_system(), AddWatcher);
  if (!file_system_instance) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }
  file_system_instance->RemoveWatcher(watcher_id, callback);
}

void ArcFileSystemOperationRunner::OnDocumentChanged(int64_t watcher_id,
                                                     mojom::ChangeType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter = watcher_callbacks_.find(watcher_id);
  if (iter == watcher_callbacks_.end()) {
    // This may happen in a race condition with documents changes and
    // RemoveWatcher().
    return;
  }
  WatcherCallback watcher_callback = iter->second;
  watcher_callback.Run(FromMojoChangeType(type));
}

void ArcFileSystemOperationRunner::OnArcPlayStoreEnabledChanged(bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnStateChanged();
}

void ArcFileSystemOperationRunner::OnInstanceReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* file_system_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->file_system(), Init);
  if (file_system_instance)
    file_system_instance->Init(binding_.CreateInterfacePtrAndBind());
  OnStateChanged();
}

void ArcFileSystemOperationRunner::OnInstanceClosed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // ArcFileSystemService and watchers are gone.
  watcher_callbacks_.clear();
  for (auto& observer : observer_list_)
    observer.OnWatchersCleared();
  OnStateChanged();
}

void ArcFileSystemOperationRunner::OnWatcherAdded(
    const WatcherCallback& watcher_callback,
    const AddWatcherCallback& callback,
    int64_t watcher_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (watcher_id < 0) {
    callback.Run(-1);
    return;
  }
  if (watcher_callbacks_.count(watcher_id)) {
    NOTREACHED();
    callback.Run(-1);
    return;
  }
  watcher_callbacks_.insert(std::make_pair(watcher_id, watcher_callback));
  callback.Run(watcher_id);
}

void ArcFileSystemOperationRunner::OnStateChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (set_should_defer_by_events_) {
    SetShouldDefer(IsArcPlayStoreEnabledForProfile(profile_) &&
                   !arc_bridge_service()->file_system()->has_instance());
  }
}

void ArcFileSystemOperationRunner::SetShouldDefer(bool should_defer) {
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
