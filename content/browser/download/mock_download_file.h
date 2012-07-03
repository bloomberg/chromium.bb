// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_FILE_H_
#pragma once

#include <string>
#include <map>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/browser/download/download_file.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

struct DownloadCreateInfo;

class MockDownloadFile : virtual public content::DownloadFile {
 public:
  MockDownloadFile();
  virtual ~MockDownloadFile();

  // DownloadFile functions.
  MOCK_METHOD0(Initialize, content::DownloadInterruptReason());
  MOCK_METHOD2(AppendDataToFile, content::DownloadInterruptReason(
      const char* data, size_t data_len));
  MOCK_METHOD1(Rename, content::DownloadInterruptReason(
      const FilePath& full_path));
  MOCK_METHOD0(Detach, void());
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD0(Finish, void());
  MOCK_METHOD0(AnnotateWithSourceInformation, void());
  MOCK_CONST_METHOD0(FullPath, FilePath());
  MOCK_CONST_METHOD0(InProgress, bool());
  MOCK_CONST_METHOD0(BytesSoFar, int64());
  MOCK_CONST_METHOD0(CurrentSpeed, int64());
  MOCK_METHOD1(GetHash, bool(std::string* hash));
  MOCK_METHOD0(GetHashState, std::string());
  MOCK_METHOD0(CancelDownloadRequest, void());
  MOCK_CONST_METHOD0(Id, int());
  MOCK_METHOD0(GetDownloadManager, content::DownloadManager*());
  MOCK_CONST_METHOD0(GlobalId, const content::DownloadId&());
  MOCK_CONST_METHOD0(DebugString, std::string());
};

#endif  // CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_FILE_H_
