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
#include "components/download/internal/scheduler/scheduler.h"
#include "components/download/internal/test/entry_utils.h"
#include "components/download/internal/test/mock_client.h"
#include "components/download/internal/test/test_device_status_listener.h"
#include "components/download/internal/test/test_download_driver.h"
#include "components/download/internal/test/test_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace download {

namespace {

bool GuidInEntryList(const std::vector<Entry>& entries,
                     const std::string& guid) {
  for (const auto& entry : entries) {
    if (entry.guid == guid)
      return true;
  }
  return false;
}

DriverEntry BuildDriverEntry(const Entry& entry, DriverEntry::State state) {
  DriverEntry dentry;
  dentry.guid = entry.guid;
  dentry.state = state;
  return dentry;
}

class MockTaskScheduler : public TaskScheduler {
 public:
  MockTaskScheduler() = default;
  ~MockTaskScheduler() override = default;

  // TaskScheduler implementation.
  MOCK_METHOD5(ScheduleTask, void(DownloadTaskType, bool, bool, long, long));
  MOCK_METHOD1(CancelTask, void(DownloadTaskType));
};

class MockScheduler : public Scheduler {
 public:
  MockScheduler() = default;
  ~MockScheduler() override = default;

  MOCK_METHOD1(Reschedule, void(const Model::EntryList&));
  MOCK_METHOD2(Next, Entry*(const Model::EntryList&, const DeviceStatus&));
};

class DownloadServiceControllerImplTest : public testing::Test {
 public:
  DownloadServiceControllerImplTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        handle_(task_runner_),
        controller_(nullptr),
        client_(nullptr),
        driver_(nullptr),
        store_(nullptr),
        model_(nullptr),
        device_status_listener_(nullptr),
        scheduler_(nullptr) {
    start_callback_ =
        base::Bind(&DownloadServiceControllerImplTest::StartCallback,
                   base::Unretained(this));
  }

  ~DownloadServiceControllerImplTest() override = default;

  void SetUp() override {
    auto client = base::MakeUnique<test::MockClient>();
    auto driver = base::MakeUnique<test::TestDownloadDriver>();
    auto store = base::MakeUnique<test::TestStore>();
    config_ = base::MakeUnique<Configuration>();

    client_ = client.get();
    driver_ = driver.get();
    store_ = store.get();

    auto clients = base::MakeUnique<DownloadClientMap>();
    clients->insert(std::make_pair(DownloadClient::TEST, std::move(client)));
    auto client_set = base::MakeUnique<ClientSet>(std::move(clients));
    auto model = base::MakeUnique<ModelImpl>(std::move(store));
    auto device_status_listener =
        base::MakeUnique<test::TestDeviceStatusListener>();
    auto scheduler = base::MakeUnique<MockScheduler>();
    auto task_scheduler = base::MakeUnique<MockTaskScheduler>();

    model_ = model.get();
    device_status_listener_ = device_status_listener.get();
    scheduler_ = scheduler.get();

    controller_ = base::MakeUnique<ControllerImpl>(
        config_.get(), std::move(client_set), std::move(driver),
        std::move(model), std::move(device_status_listener),
        std::move(scheduler), std::move(task_scheduler));
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
  std::unique_ptr<Configuration> config_;
  test::MockClient* client_;
  test::TestDownloadDriver* driver_;
  test::TestStore* store_;
  ModelImpl* model_;
  test::TestDeviceStatusListener* device_status_listener_;
  MockScheduler* scheduler_;

  DownloadParams::StartCallback start_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadServiceControllerImplTest);
};

}  // namespace

TEST_F(DownloadServiceControllerImplTest, SuccessfulInitModelFirst) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(0);

  controller_->Initialize();
  EXPECT_TRUE(store_->init_called());
  EXPECT_FALSE(controller_->GetStartupStatus()->Complete());

  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());
  EXPECT_FALSE(controller_->GetStartupStatus()->Complete());
  EXPECT_FALSE(controller_->GetStartupStatus()->driver_ok.has_value());
  EXPECT_TRUE(controller_->GetStartupStatus()->model_ok.value());

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

  driver_->MakeReady();
  EXPECT_TRUE(controller_->GetStartupStatus()->Complete());
  EXPECT_TRUE(controller_->GetStartupStatus()->driver_ok.value());
  EXPECT_TRUE(controller_->GetStartupStatus()->Ok());

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, SuccessfulInitDriverFirst) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(0);

  controller_->Initialize();
  EXPECT_TRUE(store_->init_called());
  EXPECT_FALSE(controller_->GetStartupStatus()->Complete());

  driver_->MakeReady();
  EXPECT_FALSE(controller_->GetStartupStatus()->Complete());
  EXPECT_FALSE(controller_->GetStartupStatus()->model_ok.has_value());
  EXPECT_TRUE(controller_->GetStartupStatus()->driver_ok.value());

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());
  EXPECT_TRUE(controller_->GetStartupStatus()->Complete());
  EXPECT_TRUE(controller_->GetStartupStatus()->model_ok.value());
  EXPECT_TRUE(controller_->GetStartupStatus()->Ok());

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, SuccessfulInitWithExistingDownload) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  Entry entry3 =
      test::BuildEntry(DownloadClient::INVALID, base::GenerateGUID());

  std::vector<Entry> entries = {entry1, entry2, entry3};
  std::vector<std::string> expected_guids = {entry1.guid, entry2.guid};

  EXPECT_CALL(
      *client_,
      OnServiceInitialized(testing::UnorderedElementsAreArray(expected_guids)));
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

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
  Entry entry = test::BuildBasicEntry();
  std::vector<Entry> entries = {entry};

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

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
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

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
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);
  controller_->StartDownload(params);

  // TODO(dtrainor): Compare the full DownloadParams with the full Entry.
  store_->TriggerUpdate(true);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithBackoff) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

  Entry entry = test::BuildBasicEntry();
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

  EXPECT_FALSE(GuidInEntryList(store_->updated_entries(), params.guid));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest,
       AddDownloadFailsWithDuplicateGuidInModel) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

  Entry entry = test::BuildBasicEntry();
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

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithDuplicateCall) {
  testing::InSequence sequence;

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

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

  EXPECT_TRUE(GuidInEntryList(store_->updated_entries(), params.guid));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithBadClient) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

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
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

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
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

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

TEST_F(DownloadServiceControllerImplTest, Pause) {
  // Setup download service test data.
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  Entry entry3 = test::BuildBasicEntry();
  entry1.state = Entry::State::AVAILABLE;
  entry2.state = Entry::State::ACTIVE;
  entry3.state = Entry::State::COMPLETE;
  std::vector<Entry> entries = {entry1, entry2, entry3};

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(3);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(3);

  // Set the network status to disconnected so no entries will be polled from
  // the scheduler.
  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));
  driver_->MakeReady();

  // Setup download driver test data.
  DriverEntry driver_entry1, driver_entry2, driver_entry3;
  driver_entry1.guid = entry1.guid;
  driver_entry1.state = DriverEntry::State::IN_PROGRESS;
  driver_entry2.guid = entry2.guid;
  driver_entry2.state = DriverEntry::State::IN_PROGRESS;
  driver_entry3.guid = entry3.guid;
  driver_->AddTestData(
      std::vector<DriverEntry>{driver_entry1, driver_entry2, driver_entry3});

  // Pause in progress available entry.
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entry1.guid)->state);
  controller_->PauseDownload(entry1.guid);
  EXPECT_TRUE(driver_->Find(entry1.guid)->paused);
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entry1.guid)->state);

  // Pause in progress active entry.
  controller_->PauseDownload(entry2.guid);
  EXPECT_TRUE(driver_->Find(entry2.guid)->paused);
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entry2.guid)->state);

  // Entries in complete states can't be paused.
  controller_->PauseDownload(entry3.guid);
  EXPECT_FALSE(driver_->Find(entry3.guid)->paused);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entry3.guid)->state);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, Resume) {
  // Setupd download service test data.
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  entry1.state = Entry::State::PAUSED;
  entry2.state = Entry::State::ACTIVE;
  std::vector<Entry> entries = {entry1, entry2};

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(2);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(2);

  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));
  driver_->MakeReady();

  // Setup download driver test data.
  DriverEntry driver_entry1, driver_entry2;
  driver_entry1.guid = entry1.guid;
  driver_entry1.paused = true;
  driver_entry2.guid = entry2.guid;
  driver_entry2.paused = false;
  driver_->AddTestData(std::vector<DriverEntry>{driver_entry1, driver_entry2});

  // Resume the paused download.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entry1.guid)->state);
  controller_->ResumeDownload(entry1.guid);
  EXPECT_FALSE(driver_->Find(entry1.guid)->paused);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry1.guid)->state);

  // Entries in paused state can't be resumed.
  controller_->ResumeDownload(entry2.guid);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry2.guid)->state);
  EXPECT_FALSE(driver_->Find(entry2.guid)->paused);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, Cancel) {
  Entry entry = test::BuildBasicEntry();
  entry.state = Entry::State::ACTIVE;
  std::vector<Entry> entries = {entry};

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*client_,
              OnDownloadFailed(entry.guid, Client::FailureReason::CANCELLED))
      .Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(2);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(2);

  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));
  driver_->MakeReady();

  DriverEntry driver_entry;
  driver_entry.guid = entry.guid;
  driver_->AddTestData(std::vector<DriverEntry>{driver_entry});

  controller_->CancelDownload(entry.guid);
  EXPECT_EQ(nullptr, model_->Get(entry.guid));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, OnDownloadFailed) {
  Entry entry = test::BuildBasicEntry();
  entry.state = Entry::State::ACTIVE;
  std::vector<Entry> entries = {entry};

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*client_,
              OnDownloadFailed(entry.guid, Client::FailureReason::NETWORK))
      .Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(2);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(2);

  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));
  driver_->MakeReady();

  DriverEntry driver_entry;
  driver_entry.guid = entry.guid;

  driver_->NotifyDownloadFailed(driver_entry, 1);
  EXPECT_EQ(nullptr, model_->Get(entry.guid));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, OnDownloadSucceeded) {
  Entry entry = test::BuildBasicEntry();
  entry.state = Entry::State::ACTIVE;
  std::vector<Entry> entries = {entry};

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*client_, OnDownloadSucceeded(entry.guid, _, _)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(2);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(2);

  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));
  driver_->MakeReady();

  DriverEntry driver_entry;
  driver_entry.guid = entry.guid;
  driver_entry.bytes_downloaded = 1024;
  base::FilePath path = base::FilePath::FromUTF8Unsafe("123");

  driver_->NotifyDownloadSucceeded(driver_entry, path);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entry.guid)->state);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, OnDownloadUpdated) {
  Entry entry = test::BuildBasicEntry();
  entry.state = Entry::State::ACTIVE;
  std::vector<Entry> entries = {entry};

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(1);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));
  driver_->MakeReady();

  DriverEntry driver_entry;
  driver_entry.state = DriverEntry::State::IN_PROGRESS;
  driver_entry.guid = entry.guid;
  driver_entry.bytes_downloaded = 1024;

  EXPECT_CALL(*client_,
              OnDownloadUpdated(entry.guid, driver_entry.bytes_downloaded));
  driver_->NotifyDownloadUpdate(driver_entry);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry.guid)->state);
}

TEST_F(DownloadServiceControllerImplTest, DownloadCompletionTest) {
  // TODO(dtrainor): Simulate a TIMEOUT once that is supported.
  // TODO(dtrainor): Simulate a UNKNOWN once that is supported.

  Entry entry1 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry3 = test::BuildBasicEntry(Entry::State::ACTIVE);

  DriverEntry dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::IN_PROGRESS);
  // dentry2 will effectively be created by the test to simulate a start
  // download.
  DriverEntry dentry3 =
      BuildDriverEntry(entry3, DriverEntry::State::IN_PROGRESS);

  std::vector<Entry> entries = {entry1, entry2, entry3};
  std::vector<DriverEntry> dentries = {dentry1, dentry3};

  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  // Set up the Controller.
  driver_->AddTestData(dentries);
  controller_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Test FailureReason::CANCELLED.
  EXPECT_CALL(*client_,
              OnDownloadFailed(entry1.guid, Client::FailureReason::CANCELLED))
      .Times(1);
  controller_->CancelDownload(entry1.guid);

  // Test FailureReason::ABORTED.
  EXPECT_CALL(*client_, OnDownloadStarted(entry2.guid, _, _))
      .Times(1)
      .WillOnce(Return(Client::ShouldDownload::ABORT));
  EXPECT_CALL(*client_,
              OnDownloadFailed(entry2.guid, Client::FailureReason::ABORTED))
      .Times(1);
  driver_->Start(RequestParams(), entry2.guid, NO_TRAFFIC_ANNOTATION_YET);

  // Test FailureReason::NETWORK.
  EXPECT_CALL(*client_,
              OnDownloadFailed(entry3.guid, Client::FailureReason::NETWORK))
      .Times(1);
  driver_->NotifyDownloadFailed(dentry3, 1);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, StartupRecovery) {
  EXPECT_CALL(*client_, OnServiceInitialized(_)).Times(1);

  std::vector<Entry> entries;
  std::vector<DriverEntry> driver_entries;
  entries.push_back(test::BuildBasicEntry(Entry::State::NEW));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::IN_PROGRESS));
  entries.push_back(test::BuildBasicEntry(Entry::State::NEW));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::COMPLETE));
  entries.push_back(test::BuildBasicEntry(Entry::State::NEW));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::CANCELLED));
  entries.push_back(test::BuildBasicEntry(Entry::State::NEW));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::INTERRUPTED));
  entries.push_back(test::BuildBasicEntry(Entry::State::NEW));

  entries.push_back(test::BuildBasicEntry(Entry::State::AVAILABLE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::IN_PROGRESS));
  entries.push_back(test::BuildBasicEntry(Entry::State::AVAILABLE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::COMPLETE));
  entries.push_back(test::BuildBasicEntry(Entry::State::AVAILABLE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::CANCELLED));
  entries.push_back(test::BuildBasicEntry(Entry::State::AVAILABLE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::INTERRUPTED));
  entries.push_back(test::BuildBasicEntry(Entry::State::AVAILABLE));

  entries.push_back(test::BuildBasicEntry(Entry::State::ACTIVE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::IN_PROGRESS));
  entries.push_back(test::BuildBasicEntry(Entry::State::ACTIVE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::COMPLETE));
  entries.push_back(test::BuildBasicEntry(Entry::State::ACTIVE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::CANCELLED));
  entries.push_back(test::BuildBasicEntry(Entry::State::ACTIVE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::INTERRUPTED));
  entries.push_back(test::BuildBasicEntry(Entry::State::ACTIVE));

  entries.push_back(test::BuildBasicEntry(Entry::State::PAUSED));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::IN_PROGRESS));
  entries.push_back(test::BuildBasicEntry(Entry::State::PAUSED));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::COMPLETE));
  entries.push_back(test::BuildBasicEntry(Entry::State::PAUSED));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::CANCELLED));
  entries.push_back(test::BuildBasicEntry(Entry::State::PAUSED));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::INTERRUPTED));
  entries.push_back(test::BuildBasicEntry(Entry::State::PAUSED));

  entries.push_back(test::BuildBasicEntry(Entry::State::COMPLETE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::IN_PROGRESS));
  entries.push_back(test::BuildBasicEntry(Entry::State::COMPLETE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::COMPLETE));
  entries.push_back(test::BuildBasicEntry(Entry::State::COMPLETE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::CANCELLED));
  entries.push_back(test::BuildBasicEntry(Entry::State::COMPLETE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::INTERRUPTED));
  entries.push_back(test::BuildBasicEntry(Entry::State::COMPLETE));

  // Set up the Controller.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  controller_->Initialize();
  driver_->AddTestData(driver_entries);
  driver_->MakeReady();
  store_->AutomaticallyTriggerAllFutureCallbacks(true);
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));

  // Allow the initialization routines and persistent layers to do their thing.
  task_runner_->RunUntilIdle();

  // Validate Model and DownloadDriver states.
  // Note that we are accessing the Model instead of the Store here to make it
  // easier to query the states.
  // TODO(dtrainor): Check more of the DriverEntry state to validate that the
  // entries are either paused or resumed accordingly.

  // Entry::State::NEW.
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[0].guid)->state);
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[1].guid)->state);
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[2].guid)->state);
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[3].guid)->state);
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[4].guid)->state);
  EXPECT_EQ(base::nullopt, driver_->Find(entries[0].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[1].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[2].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[3].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[4].guid));

  // Entry::State::AVAILABLE.
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entries[5].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[6].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[7].guid)->state);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entries[8].guid)->state);
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[9].guid)->state);
  EXPECT_NE(base::nullopt, driver_->Find(entries[5].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[6].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[7].guid));
  EXPECT_NE(base::nullopt, driver_->Find(entries[8].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[9].guid));

  // Entry::State::ACTIVE.
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entries[10].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[11].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[12].guid)->state);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entries[13].guid)->state);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entries[14].guid)->state);
  EXPECT_NE(base::nullopt, driver_->Find(entries[10].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[11].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[12].guid));
  EXPECT_NE(base::nullopt, driver_->Find(entries[13].guid));
  EXPECT_NE(base::nullopt, driver_->Find(entries[14].guid));

  // Entry::State::PAUSED.
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entries[15].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[16].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[17].guid)->state);
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entries[18].guid)->state);
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entries[19].guid)->state);
  EXPECT_NE(base::nullopt, driver_->Find(entries[15].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[16].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[17].guid));
  EXPECT_NE(base::nullopt, driver_->Find(entries[18].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[19].guid));

  // prog, comp, canc, int, __
  // Entry::State::COMPLETE.
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[20].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[21].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[22].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[23].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[24].guid)->state);
  EXPECT_EQ(base::nullopt, driver_->Find(entries[20].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[21].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[22].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[23].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[24].guid));
}

}  // namespace download
