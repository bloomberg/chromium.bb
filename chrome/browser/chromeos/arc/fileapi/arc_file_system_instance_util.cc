// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_instance_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

namespace file_system_instance_util {

namespace {

constexpr uint32_t kGetFileSizeVersion = 1;
constexpr uint32_t kOpenFileToReadVersion = 1;
constexpr uint32_t kGetDocumentVersion = 2;
constexpr uint32_t kGetChildDocumentsVersion = 2;

// Returns FileSystemInstance for the given |min_version|, if found.
// Otherwise, nullptr.
mojom::FileSystemInstance* GetFileSystemInstance(
    const std::string& method_name_for_logging,
    uint32_t min_version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    LOG(ERROR) << "Failed to get ArcServiceManager.";
    return nullptr;
  }
  return arc_service_manager->arc_bridge_service()
      ->file_system()
      ->GetInstanceForMethod(method_name_for_logging, min_version);
}

template <typename T>
void PostToIOThread(const base::Callback<void(T)>& callback, T result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(callback, base::Passed(std::move(result))));
}

void GetFileSizeOnUIThread(const GURL& arc_url,
                           const GetFileSizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* file_system_instance =
      GetFileSystemInstance("GetFileSize", kGetFileSizeVersion);
  if (!file_system_instance) {
    callback.Run(-1);
    return;
  }
  file_system_instance->GetFileSize(arc_url.spec(), callback);
}

void OpenFileToReadOnUIThread(const GURL& arc_url,
                              const OpenFileToReadCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* file_system_instance =
      GetFileSystemInstance("OpenFileToRead", kOpenFileToReadVersion);
  if (!file_system_instance) {
    callback.Run(mojo::ScopedHandle());
    return;
  }
  file_system_instance->OpenFileToRead(arc_url.spec(), callback);
}

void GetDocumentOnUIThread(const std::string& authority,
                           const std::string& document_id,
                           const GetDocumentCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* file_system_instance =
      GetFileSystemInstance("GetDocument", kGetDocumentVersion);
  if (!file_system_instance) {
    callback.Run(mojom::DocumentPtr());
    return;
  }
  file_system_instance->GetDocument(authority, document_id, callback);
}

void GetChildDocumentsOnUIThread(const std::string& authority,
                                 const std::string& parent_document_id,
                                 const GetChildDocumentsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* file_system_instance =
      GetFileSystemInstance("GetChildDocuments", kGetChildDocumentsVersion);
  if (!file_system_instance) {
    callback.Run(base::nullopt);
    return;
  }
  file_system_instance->GetChildDocuments(authority, parent_document_id,
                                          callback);
}

}  // namespace

void GetFileSizeOnIOThread(const GURL& arc_url,
                           const GetFileSizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetFileSizeOnUIThread, arc_url,
                 base::Bind(&PostToIOThread<int64_t>, callback)));
}

void OpenFileToReadOnIOThread(const GURL& arc_url,
                              const OpenFileToReadCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OpenFileToReadOnUIThread, arc_url,
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

}  // namespace file_system_instance_util

}  // namespace arc
