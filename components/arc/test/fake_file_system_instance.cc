// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_file_system_instance.h"

namespace arc {

FakeFileSystemInstance::FakeFileSystemInstance() = default;

FakeFileSystemInstance::~FakeFileSystemInstance() = default;

void FakeFileSystemInstance::GetFileSize(const std::string& url,
                                         const GetFileSizeCallback& callback) {}

void FakeFileSystemInstance::OpenFileToRead(
    const std::string& url,
    const OpenFileToReadCallback& callback) {}

void FakeFileSystemInstance::RequestMediaScan(
    const std::vector<std::string>& paths) {}

}  // namespace arc
