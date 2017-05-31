// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/controller_impl.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/internal/client_set.h"
#include "components/download/internal/config.h"
#include "components/download/internal/entry.h"
#include "components/download/internal/model_impl.h"
#include "components/download/internal/test/entry_utils.h"
#include "components/download/internal/test/mock_client.h"
#include "components/download/internal/test/test_download_driver.h"
#include "components/download/internal/test/test_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace download {

namespace {

class DownloadServiceControllerImplTest : public testing::Test {
 public:
  DownloadServiceControllerImplTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        handle_(task_runner_),
        client_(nullptr),
        driver_(nullptr),
        store_(nullptr) {
    start_callback_ =
        base::Bind(&DownloadServiceControllerImplTest::StartCallback,
                   base::Unretained(this));
  }
  ~DownloadServiceControllerImplTest() override = default;

  void SetUp() override {
    auto client = base::MakeUnique<test::MockClient>();
    auto driver = base::MakeUnique<test::TestDownloadDriver>();
    auto store = base::MakeUnique<test::TestStore>();
    auto config = base::MakeUnique<Configuration>();

    client_ = client.get();
    driver_ = driver.get();
    store_ = store.get();
    config_ = config.get();

    auto clients = base::MakeUnique<DownloadClientMap>();
    clients->insert(std::make_pair(DownloadClient::TEST, std::move(client)));
    auto client_set = base::MakeUnique<ClientSet>(std::move(clients));
    auto model = base::MakeUnique<ModelImpl>(std::move(store));

    controller_ = base::MakeUnique<ControllerImpl>(
        std::move(client_set), std::move(config), std::move(driver),
        std::move(model));
  }

 protected:
  DownloadParams MakeDownloadParams() {
    DownloadParams params;
    params.client = DownloadClient::TEST;
    params.guid = base::GenerateGUID();
    params.callback = start_callback_;
    return params;
  }

  MOCK_METHOD2(StartCallback,
               void(const std::string&, DownloadParams::StartResult));

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle handle_;

  std::unique_ptr<ControllerImpl> controller_;
  Configuration* config_;
  test::MockClient* client_;
  test::TestDownloadDriver* driver_;
  test::TestStore* store_;

  DownloadParams::StartCallback start_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadServiceControllerImplTest);
};

}  // namespace

TEST_F(DownloadServiceControllerImplTest, SuccessfulInitModelFirst) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(0);

  controller_->Initialize();
  EXPECT_TRUE(store_->init_called());
  EXPECT_FALSE(controller_->GetStartupStatus().Complete());

  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());
  EXPECT_FALSE(controller_->GetStartupStatus().Complete());
  EXPECT_FALSE(controller_->GetStartupStatus().driver_ok.has_value());
  EXPECT_TRUE(controller_->GetStartupStatus().model_ok.value());

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  driver_->MakeReady();
  EXPECT_TRUE(controller_->GetStartupStatus().Complete());
  EXPECT_TRUE(controller_->GetStartupStatus().driver_ok.value());
  EXPECT_TRUE(controller_->GetStartupStatus().Ok());

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, SuccessfulInitDriverFirst) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(0);

  controller_->Initialize();
  EXPECT_TRUE(store_->init_called());
  EXPECT_FALSE(controller_->GetStartupStatus().Complete());

  driver_->MakeReady();
  EXPECT_FALSE(controller_->GetStartupStatus().Complete());
  EXPECT_FALSE(controller_->GetStartupStatus().model_ok.has_value());
  EXPECT_TRUE(controller_->GetStartupStatus().driver_ok.value());

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());
  EXPECT_TRUE(controller_->GetStartupStatus().Complete());
  EXPECT_TRUE(controller_->GetStartupStatus().model_ok.value());
  EXPECT_TRUE(controller_->GetStartupStatus().Ok());

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, SuccessfulInitWithExistingDownload) {
  Entry entry1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  Entry entry2 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  Entry entry3 =
      test::BuildEntry(DownloadClient::INVALID, base::GenerateGUID());

  std::vector<Entry> entries = {entry1, entry2, entry3};
  std::vector<std::string> expected_guids = {entry1.guid, entry2.guid};

  EXPECT_CALL(
      *client_,
      OnServiceInitialized(testing::UnorderedElementsAreArray(expected_guids)));

  controller_->Initialize();
  driver_->MakeReady();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, FailedInitWithBadModel) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(0);

  controller_->Initialize();
  store_->TriggerInit(false, base::MakeUnique<std::vector<Entry>>());
  driver_->MakeReady();

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, GetOwnerOfDownload) {
  Entry entry = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  std::vector<Entry> entries = {entry};

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  controller_->Initialize();
  driver_->MakeReady();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));

  task_runner_->RunUntilIdle();

  EXPECT_EQ(DownloadClient::TEST, controller_->GetOwnerOfDownload(entry.guid));
  EXPECT_EQ(DownloadClient::INVALID,
            controller_->GetOwnerOfDownload(base::GenerateGUID()));
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadAccepted) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  // Set up the Controller.
  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  EXPECT_CALL(*this,
              StartCallback(params.guid, DownloadParams::StartResult::ACCEPTED))
      .Times(1);
  controller_->StartDownload(params);

  // TODO(dtrainor): Compare the full DownloadParams with the full Entry.
  store_->TriggerUpdate(true);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithBackoff) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  Entry entry = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  std::vector<Entry> entries = {entry};

  // Set up the Controller.
  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Set the failure expectations.
  config_->max_scheduled_downloads = 1U;

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  EXPECT_CALL(*this,
              StartCallback(params.guid, DownloadParams::StartResult::BACKOFF))
      .Times(1);
  controller_->StartDownload(params);

  EXPECT_TRUE(store_->updated_entries().empty());

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest,
       AddDownloadFailsWithDuplicateGuidInModel) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  Entry entry = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  std::vector<Entry> entries = {entry};

  // Set up the Controller.
  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  params.guid = entry.guid;
  EXPECT_CALL(
      *this,
      StartCallback(params.guid, DownloadParams::StartResult::UNEXPECTED_GUID))
      .Times(1);
  controller_->StartDownload(params);

  EXPECT_TRUE(store_->updated_entries().empty());

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithDuplicateCall) {
  testing::InSequence sequence;

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  // Set up the Controller.
  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download twice.
  DownloadParams params = MakeDownloadParams();
  EXPECT_CALL(
      *this,
      StartCallback(params.guid, DownloadParams::StartResult::UNEXPECTED_GUID))
      .Times(1);
  EXPECT_CALL(*this,
              StartCallback(params.guid, DownloadParams::StartResult::ACCEPTED))
      .Times(1);
  controller_->StartDownload(params);
  controller_->StartDownload(params);
  store_->TriggerUpdate(true);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithBadClient) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  // Set up the Controller.
  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  params.client = DownloadClient::INVALID;
  EXPECT_CALL(*this,
              StartCallback(params.guid,
                            DownloadParams::StartResult::UNEXPECTED_CLIENT))
      .Times(1);
  controller_->StartDownload(params);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithClientCancel) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  // Set up the Controller.
  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  EXPECT_CALL(
      *this,
      StartCallback(params.guid, DownloadParams::StartResult::CLIENT_CANCELLED))
      .Times(1);
  controller_->StartDownload(params);

  controller_->CancelDownload(params.guid);
  store_->TriggerUpdate(true);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithInternalError) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  // Set up the Controller.
  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  EXPECT_CALL(*this, StartCallback(params.guid,
                                   DownloadParams::StartResult::INTERNAL_ERROR))
      .Times(1);
  controller_->StartDownload(params);

  store_->TriggerUpdate(false);

  task_runner_->RunUntilIdle();
}

}  // namespace download
