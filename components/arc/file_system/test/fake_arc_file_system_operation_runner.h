// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_FILE_SYSTEM_FAKE_ARC_FILE_SYSTEM_OPERATION_RUNNER_H_
#define COMPONENTS_ARC_FILE_SYSTEM_FAKE_ARC_FILE_SYSTEM_OPERATION_RUNNER_H_

#include "base/macros.h"
#include "components/arc/file_system/arc_file_system_operation_runner.h"

namespace arc {

// TODO(crbug.com/683049): Implement a generic fake.
class FakeArcFileSystemOperationRunner : public ArcFileSystemOperationRunner {
 public:
  explicit FakeArcFileSystemOperationRunner(ArcBridgeService* bridge_service);
  ~FakeArcFileSystemOperationRunner() override;

  // ArcFileSystemOperationRunner overrides.
  void GetFileSize(const GURL& url,
                   const GetFileSizeCallback& callback) override;
  void OpenFileToRead(const GURL& url,
                      const OpenFileToReadCallback& callback) override;
  void GetDocument(const std::string& authority,
                   const std::string& document_id,
                   const GetDocumentCallback& callback) override;
  void GetChildDocuments(const std::string& authority,
                         const std::string& parent_document_id,
                         const GetChildDocumentsCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeArcFileSystemOperationRunner);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_FILE_SYSTEM_FAKE_ARC_FILE_SYSTEM_OPERATION_RUNNER_H_
