// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner_util.h"

#include "components/arc/arc_service_manager.h"
#include "components/arc/file_system/arc_file_system_operation_runner.h"
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
    callback.Run(-1);
  }
  runner->GetFileSize(url, callback);
}

void OpenFileToReadOnUIThread(const GURL& url,
                              const OpenFileToReadCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner =
      ArcServiceManager::GetGlobalService<ArcFileSystemOperationRunner>();
  if (!runner) {
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
    callback.Run(base::nullopt);
    return;
  }
  runner->GetChildDocuments(authority, parent_document_id, callback);
}

}  // namespace

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

}  // namespace file_system_operation_runner_util

}  // namespace arc
