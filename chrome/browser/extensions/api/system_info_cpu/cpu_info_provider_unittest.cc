// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CpuInfoProvider unit tests

#include "base/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using api::experimental_system_info_cpu::CpuInfo;
using api::experimental_system_info_cpu::CpuUpdateInfo;
using content::BrowserThread;

struct TestCpuInfo {
  std::string arch_name;
  std::string model_name;
  int num_of_processors;
};

struct TestCpuTimeInfo {
  int64 user;
  int64 kernel;
  int64 idle;
};

struct TestCpuUpdateInfo {
  double average_usage;
  std::vector<double> usage_per_processor;
};

const struct TestCpuInfo kTestingCpuInfoData = {
  "x86", "Intel Core", 2
};

const struct TestCpuTimeInfo kTestingCpuTimeDataForOneProcessor[][3] = {
  {{2, 4, 5}, {5, 7, 9}, {5, 7, 9}}
};

const struct TestCpuTimeInfo kTestingCpuTimeDataForTwoProcessors[][3] = {
  {{2, 4, 5}, {2, 4, 5}, {3, 5, 6}}, // Processor1 cpu time data
  {{5, 7, 9}, {5, 7, 9}, {6, 8, 10}} // Processor2 cpu time data
};

// The cpu sampling interval in unittest is 1ms.
const int kWatchingIntervalMs = 1;
// The times of doing cpu sampling.
const int kSamplingTimes = 2;
// The size of cpu time data.
const int kCpuTimeDataSize = 3;

class TestCpuInfoProvider : public CpuInfoProvider {
 public:
  typedef const struct TestCpuTimeInfo (* TestingCpuTimeDataPointer)[3];

  TestCpuInfoProvider();
  TestCpuInfoProvider(int num_of_processors,
                      TestingCpuTimeDataPointer cpu_time_data,
                      size_t sampling_interval);
  virtual bool QueryInfo(CpuInfo* info) OVERRIDE;
  virtual bool QueryCpuTimePerProcessor(std::vector<CpuTime>* times) OVERRIDE;

  // Check whether finishing cpu sampling.
  bool is_complete_sampling() {
    return query_times_ >= kCpuTimeDataSize;
  }

  int num_of_processors() const {
    return num_of_processors_;
  }

  TestingCpuTimeDataPointer cpu_time_data() const {
    return cpu_time_data_;
  }

 private:
  virtual ~TestCpuInfoProvider();

  // Record the query times of cpu_time_data_.
  int query_times_;
  int num_of_processors_;

  // A Pointer to kTestingCpuTimeDataFor*Processors array.
  TestingCpuTimeDataPointer cpu_time_data_;
};

TestCpuInfoProvider::TestCpuInfoProvider()
    : query_times_(0),
      num_of_processors_(1),
      cpu_time_data_(NULL) {
}

TestCpuInfoProvider::TestCpuInfoProvider(int num_of_processors,
    TestingCpuTimeDataPointer cpu_time_data,
    size_t sampling_interval)
    : query_times_(0),
      num_of_processors_(num_of_processors),
      cpu_time_data_(cpu_time_data) {
  sampling_interval_ = sampling_interval;
}

TestCpuInfoProvider::~TestCpuInfoProvider() {}

bool TestCpuInfoProvider::QueryInfo(CpuInfo* info) {
  if (info == NULL)
    return false;
  info->arch_name = kTestingCpuInfoData.arch_name;
  info->model_name = kTestingCpuInfoData.model_name;
  info->num_of_processors = kTestingCpuInfoData.num_of_processors;
  return true;
}

bool TestCpuInfoProvider::QueryCpuTimePerProcessor(
    std::vector<CpuTime>* times) {
  if (is_complete_sampling())
    return false;

  std::vector<CpuTime> results;
  for (int i = 0; i < num_of_processors_; ++i) {
    CpuTime cpu_time;
    cpu_time.user = cpu_time_data_[i][query_times_].user;
    cpu_time.kernel = cpu_time_data_[i][query_times_].kernel;
    cpu_time.idle = cpu_time_data_[i][query_times_].idle;
    results.push_back(cpu_time);
  }
  ++query_times_;
  times->swap(results);

  return true;
}

class CpuInfoProviderTest : public testing::Test {
 public:
  CpuInfoProviderTest();

  // A callback function to be called when CpuInfoProvider completes to sample.
  // Called on FILE thread.
  void OnCheckCpuSamplingFinishedForTesting(scoped_ptr<CpuUpdateInfo> info);
  void CalculateExpectedResult();
  void VerifyResult();

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_refptr<TestCpuInfoProvider> cpu_info_provider_;

  // Maintain the CpuUpdateInfo results returned by DoSample function.
  std::vector<TestCpuUpdateInfo> cpu_update_result_;

  // Maintain the expected CpuUpdateInfo.
  std::vector<TestCpuUpdateInfo> expected_cpu_update_result_;
};

CpuInfoProviderTest::CpuInfoProviderTest()
    : message_loop_(MessageLoop::TYPE_UI),
      ui_thread_(BrowserThread::UI, &message_loop_),
      file_thread_(BrowserThread::FILE, &message_loop_) {
}

void CpuInfoProviderTest::OnCheckCpuSamplingFinishedForTesting(
    scoped_ptr<CpuUpdateInfo> info) {
  // The cpu sampling is doing in FILE thread, so the callback function
  // should be also called in FILE thread.
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Once the sampling completed, we need to quit the FILE thread to given
  // UI thread a chance to verify results.
  if (cpu_info_provider_->is_complete_sampling()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            MessageLoop::QuitClosure());
  }

  TestCpuUpdateInfo result;
  result.average_usage = info->average_usage;
  result.usage_per_processor = info->usage_per_processor;
  cpu_update_result_.push_back(result);
}

void CpuInfoProviderTest::CalculateExpectedResult() {
  TestCpuInfoProvider::TestingCpuTimeDataPointer cpu_time_testing_data =
      cpu_info_provider_->cpu_time_data();
  DCHECK(cpu_time_testing_data != NULL);

  int test_data_index = 0;
  std::vector<TestCpuTimeInfo> baseline_cpu_time;

  for (int i = 0; i < cpu_info_provider_->num_of_processors(); ++i)
    baseline_cpu_time.push_back(cpu_time_testing_data[i][0]);
  ++test_data_index;

  for (int i = 0; i < kSamplingTimes; ++i) {
    TestCpuUpdateInfo info;
    double total_usage = 0;
    std::vector<TestCpuTimeInfo> next_cpu_time;
    for (int j = 0; j < cpu_info_provider_->num_of_processors(); ++j)
      next_cpu_time.push_back(cpu_time_testing_data[j][test_data_index]);

    for (int j = 0; j < cpu_info_provider_->num_of_processors(); ++j) {
      double toal_time =
          (next_cpu_time[j].user - baseline_cpu_time[j].user) +
          (next_cpu_time[j].idle - baseline_cpu_time[j].idle) +
          (next_cpu_time[j].kernel - baseline_cpu_time[j].kernel);
      double idle_time = next_cpu_time[j].idle - baseline_cpu_time[j].idle;
      double usage = 0;
      if (toal_time != 0)
        usage = 100 - idle_time * 100 / toal_time;
      info.usage_per_processor.push_back(usage);
      total_usage += usage;
    }
    info.average_usage =
        total_usage / cpu_info_provider_->num_of_processors();
    expected_cpu_update_result_.push_back(info);

    ++test_data_index;
    baseline_cpu_time.swap(next_cpu_time);
  }
}

void CpuInfoProviderTest::VerifyResult() {
  EXPECT_EQ(expected_cpu_update_result_.size(), cpu_update_result_.size());
  for (size_t i = 0; i < expected_cpu_update_result_.size(); ++i) {
    EXPECT_DOUBLE_EQ(expected_cpu_update_result_[i].average_usage,
                     cpu_update_result_[i].average_usage);
    EXPECT_EQ(expected_cpu_update_result_[i].usage_per_processor.size(),
              cpu_update_result_[i].usage_per_processor.size());
    for (size_t j = 0;
         j < expected_cpu_update_result_[i].usage_per_processor.size(); ++j) {
      EXPECT_DOUBLE_EQ(expected_cpu_update_result_[i].usage_per_processor[j],
                       cpu_update_result_[i].usage_per_processor[j]);
    }
  }
}

TEST_F(CpuInfoProviderTest, QueryCpuInfo) {
  cpu_info_provider_ = new TestCpuInfoProvider();
  scoped_ptr<CpuInfo> cpu_info(new CpuInfo());
  EXPECT_TRUE(cpu_info_provider_->QueryInfo(cpu_info.get()));
  EXPECT_EQ(kTestingCpuInfoData.arch_name, cpu_info->arch_name);
  EXPECT_EQ(kTestingCpuInfoData.model_name, cpu_info->model_name);
  EXPECT_EQ(kTestingCpuInfoData.num_of_processors, cpu_info->num_of_processors);
}

TEST_F(CpuInfoProviderTest, DoOnlyOneProcessorCpuSampling) {
  cpu_info_provider_ =
      new TestCpuInfoProvider(1, // number of processors
                              kTestingCpuTimeDataForOneProcessor,
                              kWatchingIntervalMs);
  CalculateExpectedResult();
  cpu_info_provider_->StartSampling(
      base::Bind(&CpuInfoProviderTest::OnCheckCpuSamplingFinishedForTesting,
                 base::Unretained(this)));
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
  content::RunMessageLoop();
  cpu_info_provider_->StopSampling();
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
  VerifyResult();
}

TEST_F(CpuInfoProviderTest, DoTwoProcessorsCpuSampling) {
  cpu_info_provider_ =
      new TestCpuInfoProvider(2, // number of processors
                              kTestingCpuTimeDataForTwoProcessors,
                              kWatchingIntervalMs);
  CalculateExpectedResult();

  cpu_info_provider_->StartSampling(
      base::Bind(&CpuInfoProviderTest::OnCheckCpuSamplingFinishedForTesting,
                 base::Unretained(this)));
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
  content::RunMessageLoop();
  cpu_info_provider_->StopSampling();
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
  VerifyResult();
}

}
