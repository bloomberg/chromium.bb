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
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
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
  TestHeapCollector() : HeapCollector(HeapCollectionMode::kTcmalloc) {}
  explicit TestHeapCollector(HeapCollectionMode mode) : HeapCollector(mode) {}

  using HeapCollector::collection_params;
  using HeapCollector::CollectProfile;
  using HeapCollector::DumpProfileToTempFile;
  using HeapCollector::IsEnabled;
  using HeapCollector::MakeQuipperCommand;
  using HeapCollector::Mode;
  using HeapCollector::ParseAndSaveProfile;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestHeapCollector);
};

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
size_t HeapSamplingPeriod(const TestHeapCollector& collector) {
  CHECK_EQ(collector.Mode(), HeapCollectionMode::kTcmalloc)
      << "Reading heap sampling period works only with tcmalloc sampling";
  size_t sampling_period;
  CHECK(base::allocator::GetNumericProperty("tcmalloc.sampling_period_bytes",
                                            &sampling_period))
      << "Failed to read heap sampling period";
  return sampling_period;
}
#endif

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
  }

  void MakeHeapCollector(HeapCollectionMode mode) {
    heap_collector_ = std::make_unique<TestHeapCollector>(mode);
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

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
TEST_F(HeapCollectorTest, CheckSetup_Tcmalloc) {
  MakeHeapCollector(HeapCollectionMode::kTcmalloc);
  heap_collector_->Init();

  // No profiles are cached on start.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(heap_collector_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());

  // Heap sampling is enabled when no incognito window is open.
  size_t sampling_period = HeapSamplingPeriod(*heap_collector_);
  EXPECT_GT(sampling_period, 0u);
}

TEST_F(HeapCollectorTest, IncognitoWindowDisablesSamplingOnInit_Tcmalloc) {
  MakeHeapCollector(HeapCollectionMode::kTcmalloc);
  OpenBrowserWindow(/*incognito=*/true);
  heap_collector_->Init();

  // Heap sampling is disabled when an incognito session is active.
  size_t sampling_period = HeapSamplingPeriod(*heap_collector_);
  EXPECT_EQ(sampling_period, 0u);
}

TEST_F(HeapCollectorTest, IncognitoWindowPausesSampling_Tcmalloc) {
  MakeHeapCollector(HeapCollectionMode::kTcmalloc);
  heap_collector_->Init();

  // Heap sampling is enabled.
  size_t sampling_period = HeapSamplingPeriod(*heap_collector_);
  EXPECT_GT(sampling_period, 0u);

  // Opening an incognito window disables sampling.
  auto win1 = OpenBrowserWindow(/*incognito=*/true);
  sampling_period = HeapSamplingPeriod(*heap_collector_);
  EXPECT_EQ(sampling_period, 0u);

  // Opening a regular window doesn't resume sampling.
  OpenBrowserWindow(/*incognito=*/false);
  // Heap sampling is still disabled.
  sampling_period = HeapSamplingPeriod(*heap_collector_);
  EXPECT_EQ(sampling_period, 0u);

  // Open another incognito window and close the first one.
  auto win3 = OpenBrowserWindow(/*incognito=*/true);
  CloseBrowserWindow(win1);
  // Heap sampling is still disabled.
  sampling_period = HeapSamplingPeriod(*heap_collector_);
  EXPECT_EQ(sampling_period, 0u);

  // Closing the last incognito window resumes heap sampling.
  CloseBrowserWindow(win3);
  // Heap sampling is enabled.
  sampling_period = HeapSamplingPeriod(*heap_collector_);
  EXPECT_GT(sampling_period, 0u);
}

TEST_F(HeapCollectorTest, DumpProfileToTempFile_Tcmalloc) {
  MakeHeapCollector(HeapCollectionMode::kTcmalloc);
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

TEST_F(HeapCollectorTest, ParseAndSaveProfile_Tcmalloc) {
  MakeHeapCollector(HeapCollectionMode::kTcmalloc);
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
#endif

TEST_F(HeapCollectorTest, CheckSetup_ShimLayer) {
  MakeHeapCollector(HeapCollectionMode::kShimLayer);
  heap_collector_->Init();

  // No profiles are cached on start.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(heap_collector_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());

  EXPECT_TRUE(heap_collector_->IsEnabled());
}

TEST_F(HeapCollectorTest, IncognitoWindowDisablesSamplingOnInit_ShimLayer) {
  MakeHeapCollector(HeapCollectionMode::kShimLayer);
  OpenBrowserWindow(/*incognito=*/true);
  heap_collector_->Init();

  // Heap sampling is disabled when an incognito session is active.
  EXPECT_FALSE(heap_collector_->IsEnabled());
}

TEST_F(HeapCollectorTest, IncognitoWindowPausesSampling_ShimLayer) {
  MakeHeapCollector(HeapCollectionMode::kShimLayer);
  heap_collector_->Init();

  // Heap sampling is enabled.
  EXPECT_TRUE(heap_collector_->IsEnabled());

  // Opening an incognito window disables sampling and doesn't crash the test.
  auto win1 = OpenBrowserWindow(/*incognito=*/true);
  EXPECT_FALSE(heap_collector_->IsEnabled());

  // Open also a regular window. Sampling still disabled.
  OpenBrowserWindow(/*incognito=*/false);
  EXPECT_FALSE(heap_collector_->IsEnabled());

  // Open another incognito window and close the first one.
  auto win3 = OpenBrowserWindow(/*incognito=*/true);
  CloseBrowserWindow(win1);
  EXPECT_FALSE(heap_collector_->IsEnabled());

  // Closing the last incognito window resumes heap sampling, without
  // crashing the test.
  CloseBrowserWindow(win3);
  EXPECT_TRUE(heap_collector_->IsEnabled());
}

TEST_F(HeapCollectorTest, DumpProfileToTempFile_ShimLayer) {
  MakeHeapCollector(HeapCollectionMode::kShimLayer);
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
            base::NumberToString(base::GetCurrentProcId()));
}

TEST_F(HeapCollectorTest, ParseAndSaveProfile_ShimLayer) {
  MakeHeapCollector(HeapCollectionMode::kShimLayer);
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

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
TEST_F(HeapCollectorCollectionParamsTest, ParametersOverride_Tcmalloc) {
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("SamplingIntervalBytes", "800000"));
  params.insert(std::make_pair("PeriodicCollectionIntervalMs", "3600000"));
  params.insert(std::make_pair("ResumeFromSuspend::SamplingFactor", "1"));
  params.insert(std::make_pair("ResumeFromSuspend::MaxDelaySec", "10"));
  params.insert(std::make_pair("RestoreSession::SamplingFactor", "2"));
  params.insert(std::make_pair("RestoreSession::MaxDelaySec", "20"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      heap_profiling::kOOPHeapProfilingFeature, params);

  TestHeapCollector heap_collector(HeapCollectionMode::kTcmalloc);
  const auto& parsed_params = heap_collector.collection_params();

  // Not initialized yet:
  size_t sampling_period = HeapSamplingPeriod(heap_collector);
  EXPECT_NE(800000u, sampling_period);
  EXPECT_NE(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_NE(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_NE(2, parsed_params.restore_session.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);

  heap_collector.Init();

  sampling_period = HeapSamplingPeriod(heap_collector);
  EXPECT_EQ(800000u, sampling_period);
  EXPECT_EQ(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_EQ(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_EQ(2, parsed_params.restore_session.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);
}
#endif

TEST_F(HeapCollectorCollectionParamsTest, ParametersOverride_ShimLayer) {
  std::map<std::string, std::string> params;
  params.insert(std::make_pair("SamplingIntervalBytes", "800000"));
  params.insert(std::make_pair("PeriodicCollectionIntervalMs", "3600000"));
  params.insert(std::make_pair("ResumeFromSuspend::SamplingFactor", "1"));
  params.insert(std::make_pair("ResumeFromSuspend::MaxDelaySec", "10"));
  params.insert(std::make_pair("RestoreSession::SamplingFactor", "2"));
  params.insert(std::make_pair("RestoreSession::MaxDelaySec", "20"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      heap_profiling::kOOPHeapProfilingFeature, params);

  TestHeapCollector heap_collector(HeapCollectionMode::kShimLayer);
  const auto& parsed_params = heap_collector.collection_params();

  // Not initialized yet:
  EXPECT_NE(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_NE(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_NE(2, parsed_params.restore_session.sampling_factor);
  EXPECT_NE(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);

  heap_collector.Init();

  EXPECT_EQ(base::TimeDelta::FromHours(1), parsed_params.periodic_interval);
  EXPECT_EQ(1, parsed_params.resume_from_suspend.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            parsed_params.resume_from_suspend.max_collection_delay);
  EXPECT_EQ(2, parsed_params.restore_session.sampling_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(20),
            parsed_params.restore_session.max_collection_delay);
}

namespace {

std::vector<base::SamplingHeapProfiler::Sample> SamplingHeapProfilerSamples() {
  // Generate a sample using the SamplingHeapProfiler collector. Then, duplicate
  // and customize their values.
  base::SamplingHeapProfiler::Init();
  auto* collector = base::SamplingHeapProfiler::Get();
  collector->Start();

  auto* sampler = base::PoissonAllocationSampler::Get();
  sampler->SuppressRandomnessForTest(true);
  sampler->SetSamplingInterval(1000000);
  // Generate and remove a dummy sample, because the first sample is potentially
  // ignored by the SHP profiler.
  sampler->RecordAlloc(reinterpret_cast<void*>(1), 1000000,
                       base::PoissonAllocationSampler::AllocatorType::kMalloc,
                       nullptr);
  sampler->RecordFree(reinterpret_cast<void*>(1));
  // Generate a second sample that we are going to retrieve.
  sampler->RecordAlloc(reinterpret_cast<void*>(2), 1000000,
                       base::PoissonAllocationSampler::AllocatorType::kMalloc,
                       nullptr);

  auto samples = collector->GetSamples(0);
  EXPECT_EQ(1lu, samples.size());
  sampler->RecordFree(reinterpret_cast<void*>(2));
  collector->Stop();

  // Customize the sample.
  auto& sample1 = samples[0];
  sample1.size = 100;
  sample1.total = 1000;
  sample1.stack = {reinterpret_cast<void*>(0x10000),
                   reinterpret_cast<void*>(0x10100),
                   reinterpret_cast<void*>(0x10200)};

  samples.emplace_back(samples.back());
  EXPECT_EQ(2lu, samples.size());
  auto& sample2 = samples[1];
  sample2.size = 200;
  sample2.total = 2000;
  sample2.stack = {
      reinterpret_cast<void*>(0x20000), reinterpret_cast<void*>(0x20100),
      reinterpret_cast<void*>(0x20200), reinterpret_cast<void*>(0x20300)};

  samples.emplace_back(samples.back());
  EXPECT_EQ(3lu, samples.size());
  auto& sample3 = samples[2];
  sample3.size = 300;
  sample3.total = 3000;
  sample3.stack = {reinterpret_cast<void*>(0x30000),
                   reinterpret_cast<void*>(0x30100),
                   reinterpret_cast<void*>(0x30200)};

  return samples;
}

std::string GetProcMaps() {
  return R"text(304acba71000-304acba72000 ---p 00000000 00:00 0
304acba72000-304acd86a000 rw-p 00000000 00:00 0
304acd86a000-304acd86b000 ---p 00000000 00:00 0
304acd86b000-304acd88a000 rw-p 00000000 00:00 0
304acd88a000-304acd88b000 ---p 00000000 00:00 0
304acd88b000-304acd8aa000 rw-p 00000000 00:00 0
5ffa92db8000-5ffa93d15000 r--p 00000000 b3:03 71780                      /opt/google/chrome/chrome
5ffa93d15000-5ffa93d16000 r--p 00f5d000 b3:03 71780                      /opt/google/chrome/chrome
5ffa93d16000-5ffa93d17000 r--p 00f5e000 b3:03 71780                      /opt/google/chrome/chrome
5ffa93d17000-5ffa9d176000 r-xp 00f5f000 b3:03 71780                      /opt/google/chrome/chrome
5ffa9d176000-5ffa9d1ca000 rw-p 0a3be000 b3:03 71780                      /opt/google/chrome/chrome
5ffa9d1ca000-5ffa9d8ff000 r--p 0a412000 b3:03 71780                      /opt/google/chrome/chrome
5ffa9d8ff000-5ffa9db6c000 rw-p 00000000 00:00 0
7f9c9e11c000-7f9c9e11d000 ---p 00000000 00:00 0
7f9c9e11d000-7f9c9e91d000 rw-p 00000000 00:00 0                          [stack:1843]
7f9c9e91d000-7f9c9e91e000 ---p 00000000 00:00 0
7f9c9e91e000-7f9c9f11e000 rw-p 00000000 00:00 0
7f9ca0d47000-7f9ca0d4c000 r-xp 00000000 b3:03 46090                      /lib64/libnss_dns-2.27.so
7f9ca0d4c000-7f9ca0f4b000 ---p 00005000 b3:03 46090                      /lib64/libnss_dns-2.27.so
7f9ca0f4b000-7f9ca0f4c000 r--p 00004000 b3:03 46090                      /lib64/libnss_dns-2.27.so
7f9ca0f4c000-7f9ca0f4d000 rw-p 00005000 b3:03 46090                      /lib64/libnss_dns-2.27.so
7f9ca0f4d000-7f9ca114d000 rw-s 00000000 00:13 16382                      /dev/shm/.com.google.Chrome.nohIdv (deleted)
7f9ca26a0000-7f9ca26a1000 ---p 00000000 00:00 0
7f9ca26a1000-7f9ca2ea1000 rw-p 00000000 00:00 0                          [stack:1796]
7f9ca2ea1000-7f9ca2f31000 r-xp 00000000 b3:03 46120                      /lib64/libpcre.so.1.2.9
7f9ca2f31000-7f9ca2f32000 r--p 0008f000 b3:03 46120                      /lib64/libpcre.so.1.2.9
7f9ca2f32000-7f9ca2f33000 rw-p 00090000 b3:03 46120                      /lib64/libpcre.so.1.2.9
7f9ca2f33000-7f9ca302d000 r-xp 00000000 b3:03 40207                      /usr/lib64/libglib-2.0.so.0.5200.3
7f9ca302e000-7f9ca302f000 r--p 000fa000 b3:03 40207                      /usr/lib64/libglib-2.0.so.0.5200.3
7f9ca4687000-7f9ca4887000 rw-s 00000000 00:13 12699                      /dev/shm/.com.google.Chrome.KWz8dN (deleted)
7f9ca4887000-7f9ca4888000 ---p 00000000 00:00 0
7f9ca4888000-7f9ca5088000 rw-p 00000000 00:00 0                          [stack:1423]
7f9ca52c8000-7f9ca52c9000 ---p 00000000 00:00 0
7f9ca52c9000-7f9ca5ac9000 rw-p 00000000 00:00 0                          [stack:1411]
7f9ca5aec000-7f9ca5b66000 r--p 00000000 b3:03 39083                      /usr/share/fonts/roboto/Roboto-Light.ttf
7f9ca5b66000-7f9ca5d66000 rw-s 00000000 00:13 10081                      /dev/shm/.com.google.Chrome.MLuvgs (deleted)
)text";
}

}  // namespace

TEST(HeapCollectorShimLayerTest, WriteHeapProfileToFile_InvalidProcMaps) {
  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File temp(temp_path, base::File::FLAG_CREATE_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  ASSERT_TRUE(temp.created());
  ASSERT_TRUE(temp.IsValid());

  auto samples = SamplingHeapProfilerSamples();
  std::string proc_maps = "Bogus proc maps\n";
  EXPECT_FALSE(internal::WriteHeapProfileToFile(&temp, samples, proc_maps));
  temp.Close();
  base::DeleteFile(temp_path, false);
}

TEST(HeapCollectorShimLayerTest, WriteHeapProfileToFile) {
  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File temp(temp_path, base::File::FLAG_CREATE_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  ASSERT_TRUE(temp.created());
  ASSERT_TRUE(temp.IsValid());

  auto samples = SamplingHeapProfilerSamples();
  auto proc_maps = GetProcMaps();
  EXPECT_TRUE(internal::WriteHeapProfileToFile(&temp, samples, proc_maps));
  temp.Close();

  std::string got;
  EXPECT_TRUE(base::ReadFileToString(temp_path, &got));

  std::string want = R"text(heap profile: 3: 6000 [3: 6000] @ heap_v2/1
1: 1000 [1: 1000] @ 0x10000 0x10100 0x10200
1: 2000 [1: 2000] @ 0x20000 0x20100 0x20200 0x20300
1: 3000 [1: 3000] @ 0x30000 0x30100 0x30200

MAPPED_LIBRARIES:
304acba71000-304acba72000 ---p 00000000 00:00 0 
304acba72000-304acd86a000 rw-p 00000000 00:00 0 
304acd86a000-304acd86b000 ---p 00000000 00:00 0 
304acd86b000-304acd88a000 rw-p 00000000 00:00 0 
304acd88a000-304acd88b000 ---p 00000000 00:00 0 
304acd88b000-304acd8aa000 rw-p 00000000 00:00 0 
5ffa92db8000-5ffa93d15000 r--p 00000000 00:00 0 /opt/google/chrome/chrome
5ffa93d15000-5ffa93d16000 r--p 00f5d000 00:00 0 /opt/google/chrome/chrome
5ffa93d16000-5ffa93d17000 r--p 00f5e000 00:00 0 /opt/google/chrome/chrome
5ffa93d17000-5ffa9d176000 r-xp 00f5f000 00:00 0 /opt/google/chrome/chrome
5ffa9d176000-5ffa9d1ca000 rw-p 0a3be000 00:00 0 /opt/google/chrome/chrome
5ffa9d1ca000-5ffa9d8ff000 r--p 0a412000 00:00 0 /opt/google/chrome/chrome
5ffa9d8ff000-5ffa9db6c000 rw-p 00000000 00:00 0 
7f9c9e11c000-7f9c9e11d000 ---p 00000000 00:00 0 
7f9c9e11d000-7f9c9e91d000 rw-p 00000000 00:00 0 [stack:1843]
7f9c9e91d000-7f9c9e91e000 ---p 00000000 00:00 0 
7f9c9e91e000-7f9c9f11e000 rw-p 00000000 00:00 0 
7f9ca0d47000-7f9ca0d4c000 r-xp 00000000 00:00 0 /lib64/libnss_dns-2.27.so
7f9ca0d4c000-7f9ca0f4b000 ---p 00005000 00:00 0 /lib64/libnss_dns-2.27.so
7f9ca0f4b000-7f9ca0f4c000 r--p 00004000 00:00 0 /lib64/libnss_dns-2.27.so
7f9ca0f4c000-7f9ca0f4d000 rw-p 00005000 00:00 0 /lib64/libnss_dns-2.27.so
7f9ca0f4d000-7f9ca114d000 rw-- 00000000 00:00 0 /dev/shm/.com.google.Chrome.nohIdv (deleted)
7f9ca26a0000-7f9ca26a1000 ---p 00000000 00:00 0 
7f9ca26a1000-7f9ca2ea1000 rw-p 00000000 00:00 0 [stack:1796]
7f9ca2ea1000-7f9ca2f31000 r-xp 00000000 00:00 0 /lib64/libpcre.so.1.2.9
7f9ca2f31000-7f9ca2f32000 r--p 0008f000 00:00 0 /lib64/libpcre.so.1.2.9
7f9ca2f32000-7f9ca2f33000 rw-p 00090000 00:00 0 /lib64/libpcre.so.1.2.9
7f9ca2f33000-7f9ca302d000 r-xp 00000000 00:00 0 /usr/lib64/libglib-2.0.so.0.5200.3
7f9ca302e000-7f9ca302f000 r--p 000fa000 00:00 0 /usr/lib64/libglib-2.0.so.0.5200.3
7f9ca4687000-7f9ca4887000 rw-- 00000000 00:00 0 /dev/shm/.com.google.Chrome.KWz8dN (deleted)
7f9ca4887000-7f9ca4888000 ---p 00000000 00:00 0 
7f9ca4888000-7f9ca5088000 rw-p 00000000 00:00 0 [stack:1423]
7f9ca52c8000-7f9ca52c9000 ---p 00000000 00:00 0 
7f9ca52c9000-7f9ca5ac9000 rw-p 00000000 00:00 0 [stack:1411]
7f9ca5aec000-7f9ca5b66000 r--p 00000000 00:00 0 /usr/share/fonts/roboto/Roboto-Light.ttf
7f9ca5b66000-7f9ca5d66000 rw-- 00000000 00:00 0 /dev/shm/.com.google.Chrome.MLuvgs (deleted)
)text";

  EXPECT_EQ(want, got);
  base::DeleteFile(temp_path, false);
}

}  // namespace metrics
