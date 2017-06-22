// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/internal/download_driver_impl.h"

#include <memory>
#include <string>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace download {

namespace {

ACTION_P(PopulateVector, items) {
  arg0->insert(arg0->begin(), items.begin(), items.end());
}

const char kFakeGuid[] = "fake_guid";

// Matcher to compare driver entries. Not all the memeber fields are compared.
// Currently no comparison in non test code, so no operator== override for
// driver entry.
MATCHER_P(DriverEntryEuqual, entry, "") {
  return entry.guid == arg.guid && entry.state == arg.state &&
         entry.bytes_downloaded == arg.bytes_downloaded &&
         entry.current_file_path.value() == arg.current_file_path.value();
}

}  // namespace

class MockDriverClient : public DownloadDriver::Client {
 public:
  MOCK_METHOD1(OnDriverReady, void(bool));
  MOCK_METHOD1(OnDownloadCreated, void(const DriverEntry&));
  MOCK_METHOD2(OnDownloadFailed, void(const DriverEntry&, int));
  MOCK_METHOD1(OnDownloadSucceeded, void(const DriverEntry&));
  MOCK_METHOD1(OnDownloadUpdated, void(const DriverEntry&));
};

class DownloadDriverImplTest : public testing::Test {
 public:
  DownloadDriverImplTest() = default;
  ~DownloadDriverImplTest() override = default;

  void SetUp() override {
    driver_ = base::MakeUnique<DownloadDriverImpl>(&mock_manager_);
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
  EXPECT_CALL(mock_client_, OnDownloadSucceeded(DriverEntryEuqual(entry)))
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

TEST_F(DownloadDriverImplTest, TestGetActiveDownloadsCall) {
  using DownloadState = content::DownloadItem::DownloadState;
  content::FakeDownloadItem item1;
  item1.SetState(DownloadState::IN_PROGRESS);
  item1.SetGuid(base::GenerateGUID());

  content::FakeDownloadItem item2;
  item2.SetState(DownloadState::CANCELLED);
  item2.SetGuid(base::GenerateGUID());

  content::FakeDownloadItem item3;
  item3.SetState(DownloadState::COMPLETE);
  item3.SetGuid(base::GenerateGUID());

  content::FakeDownloadItem item4;
  item4.SetState(DownloadState::INTERRUPTED);
  item4.SetGuid(base::GenerateGUID());

  std::vector<content::DownloadItem*> items{&item1, &item2, &item3, &item4};

  ON_CALL(mock_manager_, GetAllDownloads(_))
      .WillByDefault(PopulateVector(items));

  EXPECT_CALL(mock_manager_, IsManagerInitialized())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_client_, OnDriverReady(true)).Times(1);
  driver_->Initialize(&mock_client_);

  auto guids = driver_->GetActiveDownloads();

  EXPECT_EQ(1U, guids.size());
  EXPECT_NE(guids.end(), guids.find(item1.GetGuid()));
}

}  // namespace download
