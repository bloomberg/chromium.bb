// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/file_system/test/fake_arc_file_system_operation_runner.h"

#include "base/logging.h"

namespace arc {

FakeArcFileSystemOperationRunner::FakeArcFileSystemOperationRunner(
    ArcBridgeService* bridge_service)
    : ArcFileSystemOperationRunner(bridge_service) {}

FakeArcFileSystemOperationRunner::~FakeArcFileSystemOperationRunner() = default;

void FakeArcFileSystemOperationRunner::GetFileSize(
    const GURL& url,
    const GetFileSizeCallback& callback) {
  NOTREACHED();
}

void FakeArcFileSystemOperationRunner::OpenFileToRead(
    const GURL& url,
    const OpenFileToReadCallback& callback) {
  NOTREACHED();
}

void FakeArcFileSystemOperationRunner::GetDocument(
    const std::string& authority,
    const std::string& document_id,
    const GetDocumentCallback& callback) {
  NOTREACHED();
}

void FakeArcFileSystemOperationRunner::GetChildDocuments(
    const std::string& authority,
    const std::string& parent_document_id,
    const GetChildDocumentsCallback& callback) {
  NOTREACHED();
}

}  // namespace arc
