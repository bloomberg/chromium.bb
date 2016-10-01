// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/postmortem_report_collector.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/debug/activity_tracker.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/threading/platform_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace browser_watcher {

using base::debug::Activity;
using base::debug::ActivityData;
using base::debug::GlobalActivityTracker;
using base::debug::ThreadActivityTracker;
using base::File;
using base::FilePersistentMemoryAllocator;
using base::MemoryMappedFile;
using base::PersistentMemoryAllocator;
using base::WrapUnique;
using crashpad::CrashReportDatabase;
using crashpad::Settings;
using crashpad::UUID;
using testing::_;
using testing::Return;
using testing::SetArgPointee;

namespace {

const char kProductName[] = "TestProduct";
const char kVersionNumber[] = "TestVersionNumber";
const char kChannelName[] = "TestChannel";

// Exposes a public constructor in order to create a dummy database.
class MockCrashReportDatabase : public CrashReportDatabase {
 public:
  MockCrashReportDatabase() {}
  MOCK_METHOD0(GetSettings, Settings*());
  MOCK_METHOD1(PrepareNewCrashReport,
               CrashReportDatabase::CrashReportDatabase::OperationStatus(
                   NewReport** report));
  MOCK_METHOD2(FinishedWritingCrashReport,
               CrashReportDatabase::CrashReportDatabase::OperationStatus(
                   CrashReportDatabase::NewReport* report,
                   crashpad::UUID* uuid));
  MOCK_METHOD1(ErrorWritingCrashReport,
               CrashReportDatabase::CrashReportDatabase::OperationStatus(
                   NewReport* report));
  MOCK_METHOD2(LookUpCrashReport,
               CrashReportDatabase::CrashReportDatabase::OperationStatus(
                   const UUID& uuid,
                   Report* report));
  MOCK_METHOD1(
      GetPendingReports,
      CrashReportDatabase::OperationStatus(std::vector<Report>* reports));
  MOCK_METHOD1(
      GetCompletedReports,
      CrashReportDatabase::OperationStatus(std::vector<Report>* reports));
  MOCK_METHOD2(GetReportForUploading,
               CrashReportDatabase::OperationStatus(const UUID& uuid,
                                                    const Report** report));
  MOCK_METHOD3(RecordUploadAttempt,
               CrashReportDatabase::OperationStatus(const Report* report,
                                                    bool successful,
                                                    const std::string& id));
  MOCK_METHOD2(SkipReportUpload,
               CrashReportDatabase::OperationStatus(
                   const UUID& uuid,
                   crashpad::Metrics::CrashSkippedReason reason));
  MOCK_METHOD1(DeleteReport,
               CrashReportDatabase::OperationStatus(const UUID& uuid));
  MOCK_METHOD1(RequestUpload,
               CrashReportDatabase::OperationStatus(const UUID& uuid));
};

// Used for testing CollectAndSubmitForUpload.
class MockPostmortemReportCollector : public PostmortemReportCollector {
 public:
  MockPostmortemReportCollector()
      : PostmortemReportCollector(kProductName, kVersionNumber, kChannelName) {}

  // A function that returns a unique_ptr cannot be mocked, so mock a function
  // that returns a raw pointer instead.
  CollectionStatus Collect(const base::FilePath& debug_state_file,
                           std::unique_ptr<StabilityReport>* report) override {
    DCHECK_NE(nullptr, report);
    report->reset(CollectRaw(debug_state_file));
    return SUCCESS;
  }

  MOCK_METHOD3(GetDebugStateFilePaths,
               std::vector<base::FilePath>(
                   const base::FilePath& debug_info_dir,
                   const base::FilePath::StringType& debug_file_pattern,
                   const std::set<base::FilePath>&));
  MOCK_METHOD1(CollectRaw, StabilityReport*(const base::FilePath&));
  MOCK_METHOD4(WriteReportToMinidump,
               bool(const StabilityReport& report,
                    const crashpad::UUID& client_id,
                    const crashpad::UUID& report_id,
                    base::PlatformFile minidump_file));
};

// Checks if two proto messages are the same based on their serializations. Note
// this only works if serialization is deterministic, which is not guaranteed.
// In practice, serialization is deterministic (even for protocol buffers with
// maps) and such matchers are common in the Chromium code base. Also note that
// in the context of this test, false positive matches are the problem and these
// are not possible (otherwise serialization would be ambiguous). False
// negatives would lead to test failure and developer action. Alternatives are:
// 1) a generic matcher (likely not possible without reflections, missing from
// lite runtime),  2) a specialized matcher or 3) implementing deterministic
// serialization.
// TODO(manzagop): switch a matcher with guarantees.
MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized;
  std::string actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace

class PostmortemReportCollectorCollectAndSubmitTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    // Create a dummy debug file.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    debug_file_ = temp_dir_.GetPath().AppendASCII("foo-1.pma");
    {
      base::ScopedFILE file(base::OpenFile(debug_file_, "w"));
      ASSERT_NE(file.get(), nullptr);
    }
    ASSERT_TRUE(base::PathExists(debug_file_));

    // Expect collection of the debug file paths.
    debug_file_pattern_ = FILE_PATH_LITERAL("foo-*.pma");
    std::vector<base::FilePath> debug_files{debug_file_};
    EXPECT_CALL(collector_,
                GetDebugStateFilePaths(debug_file_.DirName(),
                                       debug_file_pattern_, no_excluded_files_))
        .Times(1)
        .WillOnce(Return(debug_files));

    EXPECT_CALL(database_, GetSettings()).Times(1).WillOnce(Return(nullptr));

    // Expect collection to a proto of a single debug file.
    // Note: caller takes ownership.
    StabilityReport* stability_report = new StabilityReport();
    EXPECT_CALL(collector_, CollectRaw(debug_file_))
        .Times(1)
        .WillOnce(Return(stability_report));

    // Expect the call to write the proto to a minidump. This involves
    // requesting a report from the crashpad database, writing the report, then
    // finalizing it with the database.
    base::FilePath minidump_path = temp_dir_.GetPath().AppendASCII("foo-1.dmp");
    base::File minidump_file(
        minidump_path, base::File::FLAG_CREATE | base::File::File::FLAG_WRITE);
    crashpad_report_ = {minidump_file.GetPlatformFile(),
                        crashpad::UUID(UUID::InitializeWithNewTag{}),
                        minidump_path};
    EXPECT_CALL(database_, PrepareNewCrashReport(_))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<0>(&crashpad_report_),
                        Return(CrashReportDatabase::kNoError)));

    EXPECT_CALL(collector_,
                WriteReportToMinidump(EqualsProto(*stability_report), _, _,
                                      minidump_file.GetPlatformFile()))
        .Times(1)
        .WillOnce(Return(true));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath debug_file_;
  MockCrashReportDatabase database_;
  MockPostmortemReportCollector collector_;
  base::FilePath::StringType debug_file_pattern_;
  std::set<base::FilePath> no_excluded_files_;
  CrashReportDatabase::NewReport crashpad_report_;
};

TEST_F(PostmortemReportCollectorCollectAndSubmitTest,
       CollectAndSubmitForUpload) {
  EXPECT_CALL(database_, FinishedWritingCrashReport(&crashpad_report_, _))
      .Times(1)
      .WillOnce(Return(CrashReportDatabase::kNoError));

  // Run the test.
  int success_cnt = collector_.CollectAndSubmitForUpload(
      debug_file_.DirName(), debug_file_pattern_, no_excluded_files_,
      &database_);
  ASSERT_EQ(1, success_cnt);
  ASSERT_FALSE(base::PathExists(debug_file_));
}

TEST_F(PostmortemReportCollectorCollectAndSubmitTest,
       CollectAndSubmitForUploadStuckFile) {
  // Open the stability debug file to prevent its deletion.
  base::ScopedFILE file(base::OpenFile(debug_file_, "w"));
  ASSERT_NE(file.get(), nullptr);

  // Expect Crashpad is notified of an error writing the crash report.
  EXPECT_CALL(database_, ErrorWritingCrashReport(&crashpad_report_))
      .Times(1)
      .WillOnce(Return(CrashReportDatabase::kNoError));

  // Run the test.
  int success_cnt = collector_.CollectAndSubmitForUpload(
      debug_file_.DirName(), debug_file_pattern_, no_excluded_files_,
      &database_);
  ASSERT_EQ(0, success_cnt);
  ASSERT_TRUE(base::PathExists(debug_file_));
}

TEST(PostmortemReportCollectorTest, GetDebugStateFilePaths) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create files.
  std::vector<base::FilePath> expected_paths;
  std::set<base::FilePath> excluded_paths;
  {
    // Matches the pattern.
    base::FilePath path = temp_dir.GetPath().AppendASCII("foo1.pma");
    base::ScopedFILE file(base::OpenFile(path, "w"));
    ASSERT_NE(file.get(), nullptr);
    expected_paths.push_back(path);

    // Matches the pattern, but is excluded.
    path = temp_dir.GetPath().AppendASCII("foo2.pma");
    file.reset(base::OpenFile(path, "w"));
    ASSERT_NE(file.get(), nullptr);
    ASSERT_TRUE(excluded_paths.insert(path).second);

    // Matches the pattern.
    path = temp_dir.GetPath().AppendASCII("foo3.pma");
    file.reset(base::OpenFile(path, "w"));
    ASSERT_NE(file.get(), nullptr);
    expected_paths.push_back(path);

    // Does not match the pattern.
    path = temp_dir.GetPath().AppendASCII("bar.baz");
    file.reset(base::OpenFile(path, "w"));
    ASSERT_NE(file.get(), nullptr);
  }

  PostmortemReportCollector collector(kProductName, kVersionNumber,
                                      kChannelName);
  EXPECT_THAT(
      collector.GetDebugStateFilePaths(
          temp_dir.GetPath(), FILE_PATH_LITERAL("foo*.pma"), excluded_paths),
      testing::UnorderedElementsAreArray(expected_paths));
}

TEST(PostmortemReportCollectorTest, CollectEmptyFile) {
  // Create an empty file.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("empty.pma");
  {
    base::ScopedFILE file(base::OpenFile(file_path, "w"));
    ASSERT_NE(file.get(), nullptr);
  }
  ASSERT_TRUE(PathExists(file_path));

  // Validate collection: an empty file cannot suppport an analyzer.
  PostmortemReportCollector collector(kProductName, kVersionNumber,
                                      kChannelName);
  std::unique_ptr<StabilityReport> report;
  ASSERT_EQ(PostmortemReportCollector::ANALYZER_CREATION_FAILED,
            collector.Collect(file_path, &report));
}

TEST(PostmortemReportCollectorTest, CollectRandomFile) {
  // Create a file with content we don't expect to be valid for a debug file.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("invalid_content.pma");
  {
    base::ScopedFILE file(base::OpenFile(file_path, "w"));
    ASSERT_NE(file.get(), nullptr);
    // Assuming this size is greater than the minimum size of a debug file.
    std::vector<uint8_t> data(1024);
    for (size_t i = 0; i < data.size(); ++i)
      data[i] = i % UINT8_MAX;
    ASSERT_EQ(data.size(),
              fwrite(&data.at(0), sizeof(uint8_t), data.size(), file.get()));
  }
  ASSERT_TRUE(PathExists(file_path));

  // Validate collection: random content appears as though there is not
  // stability data.
  PostmortemReportCollector collector(kProductName, kVersionNumber,
                                      kChannelName);
  std::unique_ptr<StabilityReport> report;
  ASSERT_EQ(PostmortemReportCollector::DEBUG_FILE_NO_DATA,
            collector.Collect(file_path, &report));
}

namespace {

// Parameters for the activity tracking.
const size_t kFileSize = 2 * 1024;
const int kStackDepth = 4;
const uint64_t kAllocatorId = 0;
const char kAllocatorName[] = "PostmortemReportCollectorCollectionTest";

}  // namespace

class PostmortemReportCollectorCollectionTest : public testing::Test {
 public:
  // Create a proper debug file.
  void SetUp() override {
    testing::Test::SetUp();

    // Create a file backed allocator.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    debug_file_path_ = temp_dir_.GetPath().AppendASCII("debug_file.pma");
    std::unique_ptr<PersistentMemoryAllocator> allocator = CreateAllocator();
    ASSERT_NE(nullptr, allocator);

    size_t tracker_mem_size =
        ThreadActivityTracker::SizeForStackDepth(kStackDepth);
    ASSERT_GT(kFileSize, tracker_mem_size);

    // Create some debug data using trackers.
    std::unique_ptr<ThreadActivityTracker> tracker =
        CreateTracker(allocator.get(), tracker_mem_size);
    ASSERT_NE(nullptr, tracker);
    ASSERT_TRUE(tracker->IsValid());

    const void* dummy_task_origin = reinterpret_cast<void*>(0xCAFE);
    const int dummy_task_sequence_num = 42;
    tracker->PushActivity(dummy_task_origin, Activity::ACT_TASK_RUN,
                          ActivityData::ForTask(dummy_task_sequence_num));

    // TODO(manzagop): flesh out the data (more trackers and content).
  }

  std::unique_ptr<PersistentMemoryAllocator> CreateAllocator() {
    // Create the memory mapped file.
    std::unique_ptr<MemoryMappedFile> mmfile(new MemoryMappedFile());
    bool success = mmfile->Initialize(
        File(debug_file_path_, File::FLAG_CREATE | File::FLAG_READ |
                                   File::FLAG_WRITE | File::FLAG_SHARE_DELETE),
        {0, static_cast<int64_t>(kFileSize)},
        MemoryMappedFile::READ_WRITE_EXTEND);
    if (!success || !mmfile->IsValid())
      return nullptr;

    // Create a persistent memory allocator.
    if (!FilePersistentMemoryAllocator::IsFileAcceptable(*mmfile, true))
      return nullptr;
    return WrapUnique(new FilePersistentMemoryAllocator(
        std::move(mmfile), kFileSize, kAllocatorId, kAllocatorName, false));
  }

  std::unique_ptr<ThreadActivityTracker> CreateTracker(
      PersistentMemoryAllocator* allocator,
      size_t tracker_mem_size) {
    // Allocate a block of memory for the tracker to use.
    PersistentMemoryAllocator::Reference mem_reference = allocator->Allocate(
        tracker_mem_size, GlobalActivityTracker::kTypeIdActivityTracker);
    if (mem_reference == 0U)
      return nullptr;

    // Get the memory's base address.
    void* mem_base = allocator->GetAsObject<char>(
        mem_reference, GlobalActivityTracker::kTypeIdActivityTracker);
    if (mem_base == nullptr)
      return nullptr;

    // Make the allocation iterable so it can be found by other processes.
    allocator->MakeIterable(mem_reference);

    return WrapUnique(new ThreadActivityTracker(mem_base, tracker_mem_size));
  }

  const base::FilePath& debug_file_path() const { return debug_file_path_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath debug_file_path_;
};

TEST_F(PostmortemReportCollectorCollectionTest, CollectSuccess) {
  // Validate collection returns the expected report.
  PostmortemReportCollector collector(kProductName, kVersionNumber,
                                      kChannelName);
  std::unique_ptr<StabilityReport> report;
  ASSERT_EQ(PostmortemReportCollector::SUCCESS,
            collector.Collect(debug_file_path(), &report));
  ASSERT_NE(nullptr, report);

  // Build the expected report.
  StabilityReport expected_report;
  ProcessState* process_state = expected_report.add_process_states();
  ThreadState* thread_state = process_state->add_threads();
  thread_state->set_thread_name(base::PlatformThread::GetName());

  ASSERT_EQ(expected_report.SerializeAsString(), report->SerializeAsString());
}

}  // namespace browser_watcher
