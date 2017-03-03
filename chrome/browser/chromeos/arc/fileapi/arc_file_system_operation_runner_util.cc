// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner_util.h"

#include <utility>
#include <vector>

#include "components/arc/arc_service_manager.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

namespace file_system_operation_runner_util {

namespace {

template <typename T>
void PostToIOThread(const base::Callback<void(T)>& callback, T result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(callback, base::Passed(std::move(result))));
}

void GetFileSizeOnUIThread(const GURL& url,
                           const GetFileSizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner =
      ArcServiceManager::GetGlobalService<ArcFileSystemOperationRunner>();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    callback.Run(-1);
    return;
  }
  runner->GetFileSize(url, callback);
}

void OpenFileToReadOnUIThread(const GURL& url,
                              const OpenFileToReadCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner =
      ArcServiceManager::GetGlobalService<ArcFileSystemOperationRunner>();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    callback.Run(mojo::ScopedHandle());
    return;
  }
  runner->OpenFileToRead(url, callback);
}

void GetDocumentOnUIThread(const std::string& authority,
                           const std::string& document_id,
                           const GetDocumentCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner =
      ArcServiceManager::GetGlobalService<ArcFileSystemOperationRunner>();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    callback.Run(mojom::DocumentPtr());
    return;
  }
  runner->GetDocument(authority, document_id, callback);
}

void GetChildDocumentsOnUIThread(const std::string& authority,
                                 const std::string& parent_document_id,
                                 const GetChildDocumentsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner =
      ArcServiceManager::GetGlobalService<ArcFileSystemOperationRunner>();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    callback.Run(base::nullopt);
    return;
  }
  runner->GetChildDocuments(authority, parent_document_id, callback);
}

void AddWatcherOnUIThread(const std::string& authority,
                          const std::string& document_id,
                          const WatcherCallback& watcher_callback,
                          const AddWatcherCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner =
      ArcServiceManager::GetGlobalService<ArcFileSystemOperationRunner>();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    callback.Run(-1);
    return;
  }
  runner->AddWatcher(authority, document_id, watcher_callback, callback);
}

void RemoveWatcherOnUIThread(int64_t watcher_id,
                             const RemoveWatcherCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner =
      ArcServiceManager::GetGlobalService<ArcFileSystemOperationRunner>();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    callback.Run(false);
    return;
  }
  runner->RemoveWatcher(watcher_id, callback);
}

void AddObserverOnUIThread(scoped_refptr<ObserverIOThreadWrapper> observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner =
      ArcServiceManager::GetGlobalService<ArcFileSystemOperationRunner>();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    return;
  }
  runner->AddObserver(observer.get());
}

void RemoveObserverOnUIThread(scoped_refptr<ObserverIOThreadWrapper> observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner =
      ArcServiceManager::GetGlobalService<ArcFileSystemOperationRunner>();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    return;
  }
  runner->RemoveObserver(observer.get());
}

}  // namespace

ObserverIOThreadWrapper::ObserverIOThreadWrapper(
    ArcFileSystemOperationRunner::Observer* underlying_observer)
    : underlying_observer_(underlying_observer) {}

ObserverIOThreadWrapper::~ObserverIOThreadWrapper() = default;

void ObserverIOThreadWrapper::Disable() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  disabled_ = true;
}

void ObserverIOThreadWrapper::OnWatchersCleared() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ObserverIOThreadWrapper::OnWatchersClearedOnIOThread, this));
}

void ObserverIOThreadWrapper::OnWatchersClearedOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (disabled_)
    return;
  underlying_observer_->OnWatchersCleared();
}

void GetFileSizeOnIOThread(const GURL& url,
                           const GetFileSizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetFileSizeOnUIThread, url,
                 base::Bind(&PostToIOThread<int64_t>, callback)));
}

void OpenFileToReadOnIOThread(const GURL& url,
                              const OpenFileToReadCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OpenFileToReadOnUIThread, url,
                 base::Bind(&PostToIOThread<mojo::ScopedHandle>, callback)));
}

void GetDocumentOnIOThread(const std::string& authority,
                           const std::string& document_id,
                           const GetDocumentCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetDocumentOnUIThread, authority, document_id,
                 base::Bind(&PostToIOThread<mojom::DocumentPtr>, callback)));
}

void GetChildDocumentsOnIOThread(const std::string& authority,
                                 const std::string& parent_document_id,
                                 const GetChildDocumentsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &GetChildDocumentsOnUIThread, authority, parent_document_id,
          base::Bind(
              &PostToIOThread<base::Optional<std::vector<mojom::DocumentPtr>>>,
              callback)));
}

void AddWatcherOnIOThread(const std::string& authority,
                          const std::string& document_id,
                          const WatcherCallback& watcher_callback,
                          const AddWatcherCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &AddWatcherOnUIThread, authority, document_id,
          base::Bind(&PostToIOThread<ArcFileSystemOperationRunner::ChangeType>,
                     watcher_callback),
          base::Bind(&PostToIOThread<int64_t>, callback)));
}

void RemoveWatcherOnIOThread(int64_t watcher_id,
                             const RemoveWatcherCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RemoveWatcherOnUIThread, watcher_id,
                 base::Bind(&PostToIOThread<bool>, callback)));
}

void AddObserverOnIOThread(scoped_refptr<ObserverIOThreadWrapper> observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&AddObserverOnUIThread, observer));
}

void RemoveObserverOnIOThread(scoped_refptr<ObserverIOThreadWrapper> observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Disable the observer now to guarantee the underlying observer is never
  // called after this function returns.
  observer->Disable();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&RemoveObserverOnUIThread, observer));
}

}  // namespace file_system_operation_runner_util

}  // namespace arc
