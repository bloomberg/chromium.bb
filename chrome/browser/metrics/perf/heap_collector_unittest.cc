// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/heap_collector.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

// Returns a sample PerfDataProto for a heap profile.
PerfDataProto GetSampleHeapPerfDataProto() {
  PerfDataProto proto;

  // Add file attributes.
  PerfDataProto_PerfFileAttr* file_attr = proto.add_file_attrs();
  PerfDataProto_PerfEventAttr* event_attr = file_attr->mutable_attr();
  event_attr->set_type(1);    // PERF_TYPE_SOFTWARE
  event_attr->set_size(96);   // PERF_ATTR_SIZE_VER3
  event_attr->set_config(9);  // PERF_COUNT_SW_DUMMY
  event_attr->set_sample_id_all(true);
  event_attr->set_sample_period(111222);
  event_attr->set_sample_type(1 /*PERF_SAMPLE_IP*/ | 2 /*PERF_SAMPLE_TID*/ |
                              32 /*PERF_SAMPLE_CALLCHAIN*/ |
                              64 /*PERF_SAMPLE_ID*/ |
                              256 /*PERF_SAMPLE_PERIOD*/);
  event_attr->set_mmap(true);
  file_attr->add_ids(0);

  PerfDataProto_PerfEventType* event_type = proto.add_event_types();
  event_type->set_id(9);  // PERF_COUNT_SW_DUMMY

  file_attr = proto.add_file_attrs();
  event_attr = file_attr->mutable_attr();
  event_attr->set_type(1);    // PERF_TYPE_SOFTWARE
  event_attr->set_size(96);   // PERF_ATTR_SIZE_VER3
  event_attr->set_config(9);  // PERF_COUNT_SW_DUMMY
  event_attr->set_sample_id_all(true);
  event_attr->set_sample_period(111222);
  event_attr->set_sample_type(1 /*PERF_SAMPLE_IP*/ | 2 /*PERF_SAMPLE_TID*/ |
                              32 /*PERF_SAMPLE_CALLCHAIN*/ |
                              64 /*PERF_SAMPLE_ID*/ |
                              256 /*PERF_SAMPLE_PERIOD*/);
  file_attr->add_ids(1);

  event_type = proto.add_event_types();
  event_type->set_id(9);  // PERF_COUNT_SW_DUMMY

  // Add MMAP event.
  PerfDataProto_PerfEvent* event = proto.add_events();
  PerfDataProto_EventHeader* header = event->mutable_header();
  header->set_type(1);  // PERF_RECORD_MMAP
  header->set_misc(0);
  header->set_size(0);

  PerfDataProto_MMapEvent* mmap = event->mutable_mmap_event();
  mmap->set_pid(3456);
  mmap->set_tid(3456);
  mmap->set_start(0x617aa770f000);
  mmap->set_len(0x617ab0689000 - 0x617aa770f000);
  mmap->set_pgoff(16);

  PerfDataProto_SampleInfo* sample_info = mmap->mutable_sample_info();
  sample_info->set_pid(3456);
  sample_info->set_tid(3456);
  sample_info->set_id(0);

  // Add Sample events.
  event = proto.add_events();
  header = event->mutable_header();
  header->set_type(9);  // PERF_RECORD_SAMPLE
  header->set_misc(2);  // PERF_RECORD_MISC_USER
  header->set_size(0);

  double scale = 1 / (1 - exp(-(1024.00 / 2.00) / 111222.00));

  PerfDataProto_SampleEvent* sample = event->mutable_sample_event();
  sample->set_ip(0x617aae951c31);
  sample->set_pid(3456);
  sample->set_tid(3456);
  sample->set_id(0);
  sample->set_period(2 * scale);
  sample->add_callchain(static_cast<uint64_t>(-512));  // PERF_CONTEXT_USER
  sample->add_callchain(0x617aae951c31);
  sample->add_callchain(0x617aae95062e);

  event = proto.add_events();
  header = event->mutable_header();
  header->set_type(9);  // PERF_RECORD_SAMPLE
  header->set_misc(2);  // PERF_RECORD_MISC_USER
  header->set_size(0);

  sample = event->mutable_sample_event();
  sample->set_ip(0x617aae951c31);
  sample->set_pid(3456);
  sample->set_tid(3456);
  sample->set_id(1);
  sample->set_period(1024 * scale);
  sample->add_callchain(static_cast<uint64_t>(-512));  // PERF_CONTEXT_USER
  sample->add_callchain(0x617aae951c31);
  sample->add_callchain(0x617aae95062e);

  return proto;
}

// Allows access to some private methods for testing.
class TestHeapCollector : public HeapCollector {
 public:
  TestHeapCollector() {}

  using HeapCollector::collection_params;
  using HeapCollector::CollectProfile;
  using HeapCollector::DumpProfileToTempFile;
  using HeapCollector::MakeQuipperCommand;
  using HeapCollector::ParseAndSaveProfile;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestHeapCollector);
};

void ReadHeapSamplingPeriod(size_t* sampling_period) {
  ASSERT_TRUE(base::allocator::GetNumericProperty(
      "tcmalloc.sampling_period_bytes", sampling_period))
      << "Failed to read heap sampling period";
}

}  // namespace

class HeapCollectorTest : public testing::Test {
 public:
  HeapCollectorTest() {}

  // Opens a new browser window, which can be incognito, and returns a unique
  // handle for it.
  size_t OpenBrowserWindow(bool incognito) {
    auto browser_window = std::make_unique<TestBrowserWindow>();
    Profile* browser_profile =
        incognito ? profile_->GetOffTheRecordProfile() : profile_.get();
    Browser::CreateParams params(browser_profile, true);
    params.type = Browser::TYPE_TABBED;
    params.window = browser_window.get();
    auto browser = std::make_unique<Browser>(params);

    size_t handle = next_browser_id++;
    open_browsers_[handle] =
        std::make_pair(std::move(browser_window), std::move(browser));
    return handle;
  }

  // Closes the browser window with the given handle.
  void CloseBrowserWindow(size_t handle) {
    auto it = open_browsers_.find(handle);
    ASSERT_FALSE(it == open_browsers_.end());
    open_browsers_.erase(it);
  }

  void SetUp() override {
    // Instantiate a testing profile.
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();

    // Create a heap collector.
    heap_collector_ = std::make_unique<TestHeapCollector>();

    // HeapCollector requires the user to be logged in.
    heap_collector_->OnUserLoggedIn();
  }

  void TearDown() override {
    heap_collector_.reset();
    open_browsers_.clear();
    profile_.reset();
  }

 protected:
  // Needed to pass PrerenderManager's DCHECKs.
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  // The associated testing browser profile.
  std::unique_ptr<TestingProfile> profile_;

  // Keep track of the open browsers and accompanying windows.
  std::unordered_map<
      size_t,
      std::pair<std::unique_ptr<TestBrowserWindow>, std::unique_ptr<Browser>>>
      open_browsers_;
  static size_t next_browser_id;

  std::unique_ptr<TestHeapCollector> heap_collector_;

  DISALLOW_COPY_AND_ASSIGN(HeapCollectorTest);
};

size_t HeapCollectorTest::next_browser_id = 1;

TEST_F(HeapCollectorTest, CheckSetup) {
  heap_collector_->Init();

  // No profiles are cached on start.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(heap_collector_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());

  // Heap sampling is enabled when no incognito window is open.
  size_t sampling_period;
  ReadHeapSamplingPeriod(&sampling_period);
  EXPECT_GT(sampling_period, 0u);
}

TEST_F(HeapCollectorTest, IncognitoWindowDisablesSamplingOnInit) {
  OpenBrowserWindow(/*incognito=*/true);
  heap_collector_->Init();

  // Heap sampling is disabled when an incognito session is active.
  size_t sampling_period;
  ReadHeapSamplingPeriod(&sampling_period);
  EXPECT_EQ(sampling_period, 0u);
}

TEST_F(HeapCollectorTest, IncognitoWindowPausesSampling) {
  heap_collector_->Init();

  // Heap sampling is enabled.
  size_t sampling_period;
  ReadHeapSamplingPeriod(&sampling_period);
  EXPECT_GT(sampling_period, 0u);

  // Opening an incognito window disables sampling.
  auto win1 = OpenBrowserWindow(/*incognito=*/true);
  ReadHeapSamplingPeriod(&sampling_period);
  EXPECT_EQ(sampling_period, 0u);

  // Opening a regular window doesn't resume sampling.
  OpenBrowserWindow(/*incognito=*/false);
  // Heap sampling is still disabled.
  ReadHeapSamplingPeriod(&sampling_period);
  EXPECT_EQ(sampling_period, 0u);

  // Open another incognito window and close the first one.
  auto win3 = OpenBrowserWindow(/*incognito=*/true);
  CloseBrowserWindow(win1);
  // Heap sampling is still disabled.
  ReadHeapSamplingPeriod(&sampling_period);
  EXPECT_EQ(sampling_period, 0u);

  // Closing the last incognito window resumes heap sampling.
  CloseBrowserWindow(win3);
  // Heap sampling is enabled.
  ReadHeapSamplingPeriod(&sampling_period);
  EXPECT_GT(sampling_period, 0u);
}

TEST_F(HeapCollectorTest, DumpProfileToTempFile) {
  base::Optional<base::FilePath> got_path =
      heap_collector_->DumpProfileToTempFile();
  // Check that we got a path.
  ASSERT_TRUE(got_path);
  // Check that the file is readable and not empty.
  base::File temp(got_path.value(),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(temp.IsValid());
  EXPECT_GT(temp.GetLength(), 0);
  temp.Close();
  // We must be able to remove the temp file.
  ASSERT_TRUE(base::DeleteFile(got_path.value(), false));
}

TEST_F(HeapCollectorTest, MakeQuipperCommand) {
  const base::FilePath kTempProfile(
      FILE_PATH_LITERAL("/tmp/MakeQuipperCommand.test"));
  base::CommandLine got = heap_collector_->MakeQuipperCommand(kTempProfile);

  // Check that we got the correct two switch names.
  ASSERT_EQ(got.GetSwitches().size(), 2u);
  EXPECT_TRUE(got.HasSwitch("input_heap_profile"));
  EXPECT_TRUE(got.HasSwitch("pid"));

  // Check that we got the correct program name and switch values.
  EXPECT_EQ(got.GetProgram().value(), "/usr/bin/quipper");
  EXPECT_EQ(got.GetSwitchValuePath("input_heap_profile"), kTempProfile);
  EXPECT_EQ(got.GetSwitchValueASCII("pid"),
            std::to_string(base::GetCurrentProcId()));
}

TEST_F(HeapCollectorTest, ParseAndSaveProfile) {
  // Write a sample perf data proto to a temp file.
  const base::FilePath kTempProfile(
      FILE_PATH_LITERAL("/tmp/ParseAndSaveProfile.test"));
  PerfDataProto heap_proto = GetSampleHeapPerfDataProto();
  std::string serialized_proto = heap_proto.SerializeAsString();

  base::File temp(kTempProfile, base::File::FLAG_CREATE_ALWAYS |
                                    base::File::FLAG_READ |
                                    base::File::FLAG_WRITE);
  EXPECT_TRUE(temp.created());
  EXPECT_TRUE(temp.IsValid());
  int res = temp.WriteAtCurrentPos(serialized_proto.c_str(),
                                   serialized_proto.length());
  EXPECT_EQ(res, static_cast<int>(serialized_proto.length()));
  temp.Close();

  // Create a command line that copies the input file to the output.
  base::CommandLine::StringVector argv;
  argv.push_back("cat");
  argv.push_back(kTempProfile.value());
  base::CommandLine cat(argv);

  // Run the command.
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  heap_collector_->ParseAndSaveProfile(cat, kTempProfile,
                                       std::move(sampled_profile));

  // Check that the profile was cached.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(heap_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_TRUE(profile.has_ms_after_boot());
  EXPECT_TRUE(profile.has_ms_after_login());

  ASSERT_TRUE(profile.has_perf_data());
  EXPECT_EQ(serialized_proto, profile.perf_data().SerializeAsString());

  // Check that the temp profile file is removed after pending tasks complete.
  heap_collector_->Deactivate();
  test_browser_thread_bundle_.RunUntilIdle();
  temp =
      base::File(kTempProfile, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_FALSE(temp.IsValid());
}

class HeapCollectorCollectionParamsTest : public testing::Test {
 public:
  HeapCollectorCollectionParamsTest()
      : task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        task_runner_handle_(task_runner_) {}

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  DISALLOW_COPY_AND_ASSIGN(HeapCollectorCollectionParamsTest);
};

TEST_F(HeapCollectorCollectionParamsTest, Parameters_Override) {
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("SamplingIntervalBytes", "800000"));
  params.insert(std::make_pair("PeriodicCollectionIntervalMs", "3600000"));
  params.insert(std::make_pair("ResumeFromSuspend::SamplingFactor", "1"));
  params.insert(std::make_pair("ResumeFromSuspend::MaxDelaySec", "10"));
  params.insert(std::make_pair("RestoreSession::SamplingFactor", "2"));
  params.insert(std::make_pair("RestoreSession::MaxDelaySec", "20"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kCWPHeapCollection,
                                                         params);

  TestHeapCollector heap_collector;
  const auto& parsed_params = heap_collector.collection_params();

  // Not initialized yet:
  size_t sampling_period;
  ReadHeapSamplingPeriod(&sampling_period);
  EXPECT_NE(800000u, sampling_period);
  EXPECT_NE(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_NE(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_NE(2, parsed_params.restore_session.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);

  heap_collector.Init();

  ReadHeapSamplingPeriod(&sampling_period);
  EXPECT_EQ(800000u, sampling_period);
  EXPECT_EQ(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_EQ(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_EQ(2, parsed_params.restore_session.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);
}

}  // namespace metrics
