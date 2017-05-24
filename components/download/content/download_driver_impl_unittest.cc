// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/download_driver_impl.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace download {

namespace {

const char kFakeGuid[] = "fake_guid";

// Matcher to compare driver entries. Not all the memeber fields are compared.
// Currently no comparison in non test code, so no operator== override for
// driver entry.
MATCHER_P(DriverEntryEuqual, entry, "") {
  return entry.guid == arg.guid && entry.state == arg.state &&
         entry.bytes_downloaded == arg.bytes_downloaded;
}

}  // namespace

class MockDriverClient : public DownloadDriver::Client {
 public:
  MOCK_METHOD1(OnDriverReady, void(bool));
  MOCK_METHOD1(OnDownloadCreated, void(const DriverEntry&));
  MOCK_METHOD2(OnDownloadFailed, void(const DriverEntry&, int));
  MOCK_METHOD2(OnDownloadSucceeded,
               void(const DriverEntry&, const base::FilePath&));
  MOCK_METHOD1(OnDownloadUpdated, void(const DriverEntry&));
};

class DownloadDriverImplTest : public testing::Test {
 public:
  DownloadDriverImplTest() = default;
  ~DownloadDriverImplTest() override = default;

  void SetUp() override {
    driver_ =
        base::MakeUnique<DownloadDriverImpl>(&mock_manager_, base::FilePath());
  }

  // TODO(xingliu): implements test download manager for embedders to test.
  NiceMock<content::MockDownloadManager> mock_manager_;
  MockDriverClient mock_client_;
  std::unique_ptr<DownloadDriverImpl> driver_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadDriverImplTest);
};

// Ensure the download manager can be initialized after the download driver.
TEST_F(DownloadDriverImplTest, ManagerLateInitialization) {
  EXPECT_CALL(mock_manager_, IsManagerInitialized())
      .Times(1)
      .WillOnce(Return(false));
  driver_->Initialize(&mock_client_);

  EXPECT_CALL(mock_client_, OnDriverReady(true));
  static_cast<content::DownloadManager::Observer*>(driver_.get())
      ->OnManagerInitialized();
}

// Ensures download updates from download items are propagated correctly.
TEST_F(DownloadDriverImplTest, DownloadItemUpdateEvents) {
  using DownloadState = content::DownloadItem::DownloadState;
  using DownloadInterruptReason = content::DownloadInterruptReason;

  EXPECT_CALL(mock_manager_, IsManagerInitialized())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_client_, OnDriverReady(true)).Times(1);
  driver_->Initialize(&mock_client_);

  content::FakeDownloadItem fake_item;
  fake_item.SetState(DownloadState::IN_PROGRESS);
  fake_item.SetGuid(kFakeGuid);
  fake_item.SetReceivedBytes(0);
  fake_item.SetTotalBytes(1024);
  DriverEntry entry = DownloadDriverImpl::CreateDriverEntry(&fake_item);

  EXPECT_CALL(mock_client_, OnDownloadUpdated(DriverEntryEuqual(entry)))
      .Times(1)
      .RetiresOnSaturation();
  static_cast<content::DownloadItem::Observer*>(driver_.get())
      ->OnDownloadUpdated(&fake_item);

  // Nothing happens for cancelled state.
  fake_item.SetState(DownloadState::CANCELLED);
  static_cast<content::DownloadItem::Observer*>(driver_.get())
      ->OnDownloadUpdated(&fake_item);

  fake_item.SetReceivedBytes(1024);
  fake_item.SetState(DownloadState::COMPLETE);
  entry = DownloadDriverImpl::CreateDriverEntry(&fake_item);
  EXPECT_CALL(mock_client_, OnDownloadSucceeded(DriverEntryEuqual(entry), _))
      .Times(1)
      .RetiresOnSaturation();
  static_cast<content::DownloadItem::Observer*>(driver_.get())
      ->OnDownloadUpdated(&fake_item);

  fake_item.SetState(DownloadState::INTERRUPTED);
  fake_item.SetLastReason(
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT);
  entry = DownloadDriverImpl::CreateDriverEntry(&fake_item);
  int reason = static_cast<int>(
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT);
  EXPECT_CALL(mock_client_, OnDownloadFailed(DriverEntryEuqual(entry), reason))
      .Times(1)
      .RetiresOnSaturation();
  static_cast<content::DownloadItem::Observer*>(driver_.get())
      ->OnDownloadUpdated(&fake_item);
}

}  // namespace download
