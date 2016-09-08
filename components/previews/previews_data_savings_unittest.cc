// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/previews_data_savings.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "components/data_reduction_proxy/core/common/data_savings_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

class TestDataSavingsRecorder
    : public data_reduction_proxy::DataSavingsRecorder {
 public:
  TestDataSavingsRecorder()
      : data_saver_enabled_(false), data_used_(0), original_size_(0) {}
  ~TestDataSavingsRecorder() {}

  // data_reduction_proxy::DataSavingsRecorder implementation:
  bool UpdateDataSavings(const std::string& host,
                         int64_t data_used,
                         int64_t original_size) override {
    if (!data_saver_enabled_) {
      return false;
    }
    last_host_ = host;
    data_used_ += data_used;
    original_size_ += original_size;
    return true;
  }

  void set_data_saver_enabled(bool data_saver_enabled) {
    data_saver_enabled_ = data_saver_enabled;
  }

  std::string last_host() const { return last_host_; }

  int64_t data_used() const { return data_used_; }

  int64_t original_size() const { return original_size_; }

 private:
  bool data_saver_enabled_;
  std::string last_host_;
  int64_t data_used_;
  int64_t original_size_;
};

}  // namespace

class PreviewsDataSavingsTest : public testing::Test {
 public:
  PreviewsDataSavingsTest()
      : test_data_savings_recorder_(new TestDataSavingsRecorder()),
        data_savings_(
            new PreviewsDataSavings(test_data_savings_recorder_.get())) {}
  ~PreviewsDataSavingsTest() override {}

  PreviewsDataSavings* data_savings() const { return data_savings_.get(); }

  TestDataSavingsRecorder* test_data_savings_recorder() const {
    return test_data_savings_recorder_.get();
  }

 private:
  std::unique_ptr<TestDataSavingsRecorder> test_data_savings_recorder_;
  std::unique_ptr<PreviewsDataSavings> data_savings_;
};

TEST_F(PreviewsDataSavingsTest, RecordDataSavings) {
  int64_t original_size = 200;
  int64_t data_used = 100;
  std::string host = "host";

  EXPECT_EQ(0, test_data_savings_recorder()->data_used());
  EXPECT_EQ(0, test_data_savings_recorder()->original_size());
  test_data_savings_recorder()->set_data_saver_enabled(false);

  data_savings()->RecordDataSavings(host, data_used, original_size);

  EXPECT_EQ(0, test_data_savings_recorder()->data_used());
  EXPECT_EQ(0, test_data_savings_recorder()->original_size());
  EXPECT_EQ(std::string(), test_data_savings_recorder()->last_host());
  test_data_savings_recorder()->set_data_saver_enabled(true);

  data_savings()->RecordDataSavings(host, data_used, original_size);

  EXPECT_EQ(data_used, test_data_savings_recorder()->data_used());
  EXPECT_EQ(original_size, test_data_savings_recorder()->original_size());
  EXPECT_EQ(host, test_data_savings_recorder()->last_host());
}

}  // namespace previews
