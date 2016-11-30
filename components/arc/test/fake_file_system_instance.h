// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_FILE_SYSTEM_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_FILE_SYSTEM_INSTANCE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/arc/common/file_system.mojom.h"

namespace arc {

class FakeFileSystemInstance : public mojom::FileSystemInstance {
 public:
  FakeFileSystemInstance();

  // mojom::FileSystemInstance:
  ~FakeFileSystemInstance() override;

  void GetFileSize(const std::string& url,
                   const GetFileSizeCallback& callback) override;

  void OpenFileToRead(const std::string& url,
                      const OpenFileToReadCallback& callback) override;

  void RequestMediaScan(const std::vector<std::string>& paths) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeFileSystemInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_FILE_SYSTEM_INSTANCE_H_
