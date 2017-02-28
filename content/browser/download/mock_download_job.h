// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_JOB_H_
#define CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_JOB_H_

#include <string>

#include "base/macros.h"
#include "content/browser/download/download_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class DownloadItemImpl;

class MockDownloadJob : public DownloadJob {
 public:
  explicit MockDownloadJob(DownloadItemImpl* download_item);
  ~MockDownloadJob() override;

  DownloadItemImpl* download_item() { return download_item_; }

  // DownloadJob implementation.
  MOCK_METHOD0(Start, void());
  MOCK_METHOD1(Cancel, void(bool));
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(Resume, void(bool));
  MOCK_CONST_METHOD0(CanOpen, bool());
  MOCK_CONST_METHOD0(CanResume, bool());
  MOCK_CONST_METHOD0(CanShowInFolder, bool());
  MOCK_CONST_METHOD0(IsActive, bool());
  MOCK_CONST_METHOD0(IsPaused, bool());
  MOCK_CONST_METHOD0(PercentComplete, int());
  MOCK_CONST_METHOD0(CurrentSpeed, int64_t());
  MOCK_CONST_METHOD1(TimeRemaining, bool(base::TimeDelta* remaining));
  MOCK_CONST_METHOD0(GetWebContents, WebContents*());
  MOCK_CONST_METHOD1(DebugString, std::string(bool));
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_JOB_H_
