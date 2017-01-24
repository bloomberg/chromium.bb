// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_FILE_SYSTEM_ARC_FILE_SYSTEM_OPERATION_RUNNER_H_
#define COMPONENTS_ARC_FILE_SYSTEM_ARC_FILE_SYSTEM_OPERATION_RUNNER_H_

#include <string>

#include "components/arc/arc_service.h"
#include "components/arc/common/file_system.mojom.h"

class GURL;

namespace arc {

// An interface to run ARC file system operations.
//
// This is an abstraction layer on top of mojom::FileSystemInstance.
// In production, ArcDeferredFileSystemOperationRunner is always used which
// defers file system operations while ARC is booting.
// In unit tests, fake implementations will be injected.
//
// All member functions must be called on the UI thread.
class ArcFileSystemOperationRunner : public ArcService {
 public:
  // For supporting ArcServiceManager::GetService<T>().
  static const char kArcServiceName[];

  explicit ArcFileSystemOperationRunner(ArcBridgeService* bridge_service);
  ~ArcFileSystemOperationRunner() override;

  using GetFileSizeCallback = mojom::FileSystemInstance::GetFileSizeCallback;
  using OpenFileToReadCallback =
      mojom::FileSystemInstance::OpenFileToReadCallback;
  using GetDocumentCallback = mojom::FileSystemInstance::GetDocumentCallback;
  using GetChildDocumentsCallback =
      mojom::FileSystemInstance::GetChildDocumentsCallback;

  // Runs file system operations. See file_system.mojom for documentation.
  virtual void GetFileSize(const GURL& url,
                           const GetFileSizeCallback& callback) = 0;
  virtual void OpenFileToRead(const GURL& url,
                              const OpenFileToReadCallback& callback) = 0;
  virtual void GetDocument(const std::string& authority,
                           const std::string& document_id,
                           const GetDocumentCallback& callback) = 0;
  virtual void GetChildDocuments(const std::string& authority,
                                 const std::string& parent_document_id,
                                 const GetChildDocumentsCallback& callback) = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_FILE_SYSTEM_ARC_FILE_SYSTEM_OPERATION_RUNNER_H_
