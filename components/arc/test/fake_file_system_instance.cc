// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_file_system_instance.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/system/handle.h"

namespace arc {

FakeFileSystemInstance::FakeFileSystemInstance() = default;

FakeFileSystemInstance::~FakeFileSystemInstance() = default;

void FakeFileSystemInstance::GetChildDocuments(
    const std::string& authority,
    const std::string& document_id,
    const GetChildDocumentsCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::nullopt));
}

void FakeFileSystemInstance::GetDocument(const std::string& authority,
                                         const std::string& document_id,
                                         const GetDocumentCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::Passed(mojom::DocumentPtr())));
}

void FakeFileSystemInstance::GetFileSize(const std::string& url,
                                         const GetFileSizeCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, -1));
}

void FakeFileSystemInstance::OpenFileToRead(
    const std::string& url,
    const OpenFileToReadCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, base::Passed(mojo::ScopedHandle())));
}

void FakeFileSystemInstance::RequestMediaScan(
    const std::vector<std::string>& paths) {
  // Do nothing and pretend we scaned them.
}

}  // namespace arc
