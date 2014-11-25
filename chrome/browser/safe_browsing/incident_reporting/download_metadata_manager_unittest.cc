// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/download_metadata_manager.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;

namespace safe_browsing {

namespace {

const uint32_t kTestDownloadId = 47;
const uint32_t kOtherDownloadId = 48;
const uint32_t kCrazyDowloadId = 655;
const int64 kTestDownloadTimeMsec = 84;
const char kTestUrl[] = "http://test.test/foo";
const uint64_t kTestDownloadLength = 1000;
const double kTestDownloadEndTimeMs = 1413514824057;

// A utility class suitable for mocking that exposes a
// GetDownloadDetailsCallback.
class DownloadDetailsGetter {
 public:
  virtual ~DownloadDetailsGetter() {}
  virtual void OnDownloadDetails(
      ClientIncidentReport_DownloadDetails* details) = 0;
  DownloadMetadataManager::GetDownloadDetailsCallback GetCallback() {
    return base::Bind(&DownloadDetailsGetter::DownloadDetailsCallback,
                      base::Unretained(this));
  }

 private:
  void DownloadDetailsCallback(
      scoped_ptr<ClientIncidentReport_DownloadDetails> details) {
    OnDownloadDetails(details.get());
  }
};

// A mock DownloadDetailsGetter.
class MockDownloadDetailsGetter : public DownloadDetailsGetter {
 public:
  MOCK_METHOD1(OnDownloadDetails, void(ClientIncidentReport_DownloadDetails*));
};

// A mock DownloadMetadataManager that can be used to map a BrowserContext to
// a DownloadManager.
class MockDownloadMetadataManager : public DownloadMetadataManager {
 public:
  MockDownloadMetadataManager(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : DownloadMetadataManager(task_runner) {}

  MOCK_METHOD1(GetDownloadManagerForBrowserContext,
               content::DownloadManager*(content::BrowserContext*));
};

// A helper function that returns the download URL from a DownloadDetails.
const std::string& GetDetailsDownloadUrl(
    const ClientIncidentReport_DownloadDetails* details) {
  return details->download().url();
}

// A helper function that returns the open time from a DownloadDetails.
int64_t GetDetailsOpenTime(
    const ClientIncidentReport_DownloadDetails* details) {
  return details->open_time_msec();
}

}  // namespace

// The basis upon which unit tests of the DownloadMetadataManager are built.
class DownloadMetadataManagerTestBase : public ::testing::Test {
 protected:
  // Sets up a DownloadMetadataManager that will run tasks on the main test
  // thread.
  DownloadMetadataManagerTestBase()
      : manager_(scoped_refptr<base::SequencedTaskRunner>(
            base::ThreadTaskRunnerHandle::Get())),
        download_manager_(),
        dm_observer_() {}

  // Returns the path to the test profile's DownloadMetadata file.
  base::FilePath GetMetadataPath() const {
    return profile_.GetPath().Append(FILE_PATH_LITERAL("DownloadMetadata"));
  }

  // Returns a new ClientDownloadRequest for the given download URL.
  static scoped_ptr<ClientDownloadRequest> MakeTestRequest(const char* url) {
    scoped_ptr<ClientDownloadRequest> request(new ClientDownloadRequest());
    request->set_url(url);
    request->mutable_digests();
    request->set_length(kTestDownloadLength);
    return request.Pass();
  }

  // Returns a new DownloadMetdata for the given download id.
  static scoped_ptr<DownloadMetadata> GetTestMetadata(uint32_t download_id) {
    scoped_ptr<DownloadMetadata> metadata(new DownloadMetadata());
    metadata->set_download_id(download_id);
    ClientIncidentReport_DownloadDetails* details =
        metadata->mutable_download();
    details->set_download_time_msec(kTestDownloadTimeMsec);
    details->set_allocated_download(MakeTestRequest(kTestUrl).release());
    return metadata.Pass();
  }

  // Writes a test DownloadMetadata file for the given download id to the
  // test profile directory.
  void WriteTestMetadataFileForItem(uint32_t download_id) {
    std::string data;
    ASSERT_TRUE(GetTestMetadata(download_id)->SerializeToString(&data));
    ASSERT_TRUE(base::WriteFile(GetMetadataPath(), data.data(), data.size()));
  }

  // Writes a test DownloadMetadata file for kTestDownloadId to the test profile
  // directory.
  void WriteTestMetadataFile() {
    WriteTestMetadataFileForItem(kTestDownloadId);
  }

  // Returns the DownloadMetadata read from the test profile's directory.
  scoped_ptr<DownloadMetadata> ReadTestMetadataFile() const {
    std::string data;
    if (!base::ReadFileToString(GetMetadataPath(), &data))
      return scoped_ptr<DownloadMetadata>();
    scoped_ptr<DownloadMetadata> result(new DownloadMetadata);
    EXPECT_TRUE(result->ParseFromString(data));
    return result.Pass();
  }

  // Runs all tasks posted to the test thread's message loop.
  void RunAllTasks() { base::MessageLoop::current()->RunUntilIdle(); }

  // Adds a DownloadManager for the test profile. The DownloadMetadataManager's
  // observer is stashed for later use. Only call once per call to
  // ShutdownDownloadManager.
  void AddDownloadManager() {
    ASSERT_EQ(nullptr, dm_observer_);
    // Shove the manager into the browser context.
    ON_CALL(download_manager_, GetBrowserContext())
        .WillByDefault(Return(&profile_));
    ON_CALL(manager_, GetDownloadManagerForBrowserContext(Eq(&profile_)))
        .WillByDefault(Return(&download_manager_));
    // Capture the metadata manager's observer on the download manager.
    EXPECT_CALL(download_manager_, AddObserver(&manager_))
        .WillOnce(SaveArg<0>(&dm_observer_));
    manager_.AddDownloadManager(&download_manager_);
  }

  // Shuts down the DownloadManager. Safe to call any number of times.
  void ShutdownDownloadManager() {
    if (dm_observer_) {
      dm_observer_->ManagerGoingDown(&download_manager_);
      // Note: these calls may result in "Uninteresting mock function call"
      // warnings as a result of MockDownloadItem invoking observers in its
      // dtor. This happens after the NiceMock wrapper has removed its niceness
      // hook. These can safely be ignored, as they are entirely expected. The
      // values specified by ON_CALL invocations in AddDownloadItems are
      // returned as desired.
      other_item_.reset();
      test_item_.reset();
      dm_observer_ = nullptr;
    }
  }

  // Adds two test DownloadItems to the DownloadManager.
  void AddDownloadItems() {
    ASSERT_NE(nullptr, dm_observer_);
    // Add the item under test.
    test_item_.reset(new NiceMock<content::MockDownloadItem>);
    ON_CALL(*test_item_, GetId())
        .WillByDefault(Return(kTestDownloadId));
    ON_CALL(*test_item_, GetBrowserContext())
        .WillByDefault(Return(&profile_));
    ON_CALL(*test_item_, GetEndTime())
        .WillByDefault(Return(base::Time::FromJsTime(kTestDownloadEndTimeMs)));
    dm_observer_->OnDownloadCreated(&download_manager_, test_item_.get());

    // Add another item.
    other_item_.reset(new NiceMock<content::MockDownloadItem>);
    ON_CALL(*other_item_, GetId())
        .WillByDefault(Return(kOtherDownloadId));
    ON_CALL(*other_item_, GetBrowserContext())
        .WillByDefault(Return(&profile_));
    ON_CALL(*test_item_, GetEndTime())
        .WillByDefault(Return(base::Time::FromJsTime(kTestDownloadEndTimeMs)));
    dm_observer_->OnDownloadCreated(&download_manager_, other_item_.get());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  NiceMock<MockDownloadMetadataManager> manager_;
  TestingProfile profile_;
  NiceMock<content::MockDownloadManager> download_manager_;
  scoped_ptr<content::MockDownloadItem> test_item_;
  scoped_ptr<content::MockDownloadItem> other_item_;
  content::DownloadManager::Observer* dm_observer_;
};

// A parameterized test that exercises GetDownloadDetails. The parameters
// dictate the exact state of affairs leading up to the call as follows:
// 0: if "present", the profile has a pre-existing DownloadMetadata file.
// 1: if "managed", the profile's DownloadManager has been created.
// 2: the state of the DownloadItem prior to the call:
//    "not_created": the DownloadItem has not been created.
//    "created": the DownloadItem has been created.
//    "opened": the DownloadItem has been opened.
//    "removed": the DownloadItem has been removed.
// 3: if "loaded", the task to load the DownloadMetadata file is allowed to
//    complete.
// 4: if "early_shutdown", the DownloadManager is shut down before the callback
//    is allowed to complete.
class GetDetailsTest
    : public DownloadMetadataManagerTestBase,
      public ::testing::WithParamInterface<testing::tuple<const char*,
                                                          const char*,
                                                          const char*,
                                                          const char*,
                                                          const char*>> {
 protected:
  enum DownloadItemAction {
    NOT_CREATED,
    CREATED,
    OPENED,
    REMOVED,
  };
  GetDetailsTest()
      : metadata_file_present_(),
        manager_added_(),
        item_action_(NOT_CREATED),
        details_loaded_(),
        early_shutdown_() {}

  void SetUp() override {
    DownloadMetadataManagerTestBase::SetUp();
    metadata_file_present_ =
        (std::string(testing::get<0>(GetParam())) == "present");
    manager_added_ = (std::string(testing::get<1>(GetParam())) == "managed");
    const std::string item_action(testing::get<2>(GetParam()));
    item_action_ = (item_action == "not_created" ? NOT_CREATED :
                    (item_action == "created" ? CREATED :
                     (item_action == "opened" ? OPENED : REMOVED)));
    details_loaded_ = (std::string(testing::get<3>(GetParam())) == "loaded");
    early_shutdown_ =
        (std::string(testing::get<4>(GetParam())) == "early_shutdown");

    // Fixup combinations that don't make sense.
    if (!manager_added_)
      item_action_ = NOT_CREATED;
  }

  bool metadata_file_present_;
  bool manager_added_;
  DownloadItemAction item_action_;
  bool details_loaded_;
  bool early_shutdown_;
};

// Tests that DownloadMetadataManager::GetDownloadDetails works for all
// combinations of states.
TEST_P(GetDetailsTest, GetDownloadDetails) {
  // Optionally put a metadata file in the profile directory.
  if (metadata_file_present_)
    WriteTestMetadataFile();

  // Optionally add a download manager for the profile.
  if (manager_added_)
    AddDownloadManager();

  // Optionally create download items and perform actions on the one under test.
  if (item_action_ != NOT_CREATED)
    AddDownloadItems();
  if (item_action_ == OPENED)
    test_item_->NotifyObserversDownloadOpened();
  else if (item_action_ == REMOVED)
    test_item_->NotifyObserversDownloadRemoved();

  // Optionally allow the task to read the file to complete.
  if (details_loaded_)
    RunAllTasks();

  // In http://crbug.com/433928, open after removal during load caused a crash.
  if (item_action_ == REMOVED)
    test_item_->NotifyObserversDownloadOpened();

  MockDownloadDetailsGetter details_getter;
  if (metadata_file_present_ && item_action_ != REMOVED) {
    // The file is present, so expect that the callback is invoked with the
    // details of the test download data written by WriteTestMetadataFile.
    if (item_action_ == OPENED) {
      EXPECT_CALL(details_getter,
                  OnDownloadDetails(
                      AllOf(ResultOf(GetDetailsDownloadUrl, StrEq(kTestUrl)),
                            ResultOf(GetDetailsOpenTime, Ne(0)))));
    } else {
      EXPECT_CALL(details_getter,
                  OnDownloadDetails(
                      AllOf(ResultOf(GetDetailsDownloadUrl, StrEq(kTestUrl)),
                            ResultOf(GetDetailsOpenTime, Eq(0)))));
    }
  } else {
    // No file on disk, so expect that the callback is invoked with null.
    EXPECT_CALL(details_getter, OnDownloadDetails(IsNull()));
  }

  // Fire in the hole!
  manager_.GetDownloadDetails(&profile_, details_getter.GetCallback());

  // Shutdown the download manager, if relevant.
  if (early_shutdown_)
    ShutdownDownloadManager();

  // Allow the read task and the response callback to run.
  RunAllTasks();

  // Shutdown the download manager, if relevant.
  ShutdownDownloadManager();
}

INSTANTIATE_TEST_CASE_P(
    DownloadMetadataManager,
    GetDetailsTest,
    testing::Combine(
        testing::Values("absent", "present"),
        testing::Values("not_managed", "managed"),
        testing::Values("not_created", "created", "opened", "removed"),
        testing::Values("waiting", "loaded"),
        testing::Values("normal_shutdown", "early_shutdown")));

// A parameterized test that exercises SetRequest. The parameters dictate the
// exact state of affairs leading up to the call as follows:
// 0: the state of the DownloadMetadata file for the test profile:
//    "absent": no file is present.
//    "this": the file corresponds to the item being updated.
//    "other": the file correponds to a different item.
//    "unknown": the file corresponds to an item that has not been created.
// 1: if "pending", an operation is applied to the item being updated prior to
//    the call.
// 2: if "pending", an operation is applied to a different item prior to the
//    call.
// 3: if "loaded", the task to load the DownloadMetadata file is allowed to
//    complete.
// 4: if "set", the call to SetRequest contains a new request; otherwise it
//    does not, leading to removal of metadata.
class SetRequestTest
    : public DownloadMetadataManagerTestBase,
      public ::testing::WithParamInterface<testing::tuple<const char*,
                                                          const char*,
                                                          const char*,
                                                          const char*,
                                                          const char*>> {
 protected:
  enum MetadataFilePresent {
    ABSENT,
    PRESENT_FOR_THIS_ITEM,
    PRESENT_FOR_OTHER_ITEM,
    PRESENT_FOR_UNKNOWN_ITEM,
  };
  SetRequestTest()
      : metadata_file_present_(ABSENT),
        same_ops_(),
        other_ops_(),
        details_loaded_(),
        set_request_() {}

  void SetUp() override {
    DownloadMetadataManagerTestBase::SetUp();
    const std::string present(testing::get<0>(GetParam()));
    metadata_file_present_ = (present == "absent" ? ABSENT :
                              (present == "this" ? PRESENT_FOR_THIS_ITEM :
                               (present == "other" ? PRESENT_FOR_OTHER_ITEM :
                                PRESENT_FOR_UNKNOWN_ITEM)));
    same_ops_ = (std::string(testing::get<1>(GetParam())) == "pending");
    other_ops_ = (std::string(testing::get<2>(GetParam())) == "pending");
    details_loaded_ = (std::string(testing::get<3>(GetParam())) == "loaded");
    set_request_ = (std::string(testing::get<4>(GetParam())) == "set");
  }

  MetadataFilePresent metadata_file_present_;
  bool same_ops_;
  bool other_ops_;
  bool details_loaded_;
  bool set_request_;
};

// Tests that DownloadMetadataManager::SetRequest works for all combinations of
// states.
TEST_P(SetRequestTest, SetRequest) {
  // Optionally put a metadata file in the profile directory.
  switch (metadata_file_present_) {
    case ABSENT:
      break;
    case PRESENT_FOR_THIS_ITEM:
      WriteTestMetadataFile();
      break;
    case PRESENT_FOR_OTHER_ITEM:
      WriteTestMetadataFileForItem(kOtherDownloadId);
      break;
    case PRESENT_FOR_UNKNOWN_ITEM:
      WriteTestMetadataFileForItem(kCrazyDowloadId);
      break;
  }

  AddDownloadManager();
  AddDownloadItems();

  // Optionally allow the task to read the file to complete.
  if (details_loaded_) {
    RunAllTasks();
  } else {
    // Optionally add pending operations if the load is outstanding.
    if (same_ops_)
      test_item_->NotifyObserversDownloadOpened();
    if (other_ops_)
      other_item_->NotifyObserversDownloadOpened();
  }

  static const char kNewUrl[] = "http://blorf";
  scoped_ptr<ClientDownloadRequest> request;
  if (set_request_)
    request = MakeTestRequest(kNewUrl).Pass();
  else
    request.reset();
  manager_.SetRequest(test_item_.get(), request.get());

  // Allow the write or remove task to run.
  RunAllTasks();

  MockDownloadDetailsGetter details_getter;
  if (set_request_) {
    // Expect that the callback is invoked with details for this item.
    EXPECT_CALL(
        details_getter,
        OnDownloadDetails(ResultOf(GetDetailsDownloadUrl, StrEq(kNewUrl))));
  } else {
    // Expect that the callback is invoked with null to clear stale metadata.
    EXPECT_CALL(details_getter, OnDownloadDetails(IsNull()));
  }
  manager_.GetDownloadDetails(&profile_, details_getter.GetCallback());

  // In http://crbug.com/433928, open after SetRequest(nullpr) caused a crash.
  test_item_->NotifyObserversDownloadOpened();

  ShutdownDownloadManager();

  scoped_ptr<DownloadMetadata> metadata(ReadTestMetadataFile());
  if (set_request_) {
    // Expect that the file contains metadata for the download.
    ASSERT_TRUE(metadata);
    EXPECT_EQ(kTestDownloadId, metadata->download_id());
    EXPECT_STREQ(kNewUrl, metadata->download().download().url().c_str());
  } else {
    // Expect that the file is not present.
    ASSERT_FALSE(metadata);
  }
}

INSTANTIATE_TEST_CASE_P(
    DownloadMetadataManager,
    SetRequestTest,
    testing::Combine(testing::Values("absent", "this", "other", "unknown"),
                     testing::Values("none", "pending"),
                     testing::Values("none", "pending"),
                     testing::Values("waiting", "loaded"),
                     testing::Values("clear", "set")));

}  // namespace safe_browsing
