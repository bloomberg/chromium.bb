// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_als_reader.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

class TestObserver : public AlsReader::Observer {
 public:
  TestObserver() {}
  ~TestObserver() override = default;

  // AlsReader::Observer overrides:
  void OnAmbientLightUpdated(const int lux) override {
    ambient_light_ = lux;
    ++num_received_ambient_lights_;
  }

  void OnAlsReaderInitialized(const AlsReader::AlsInitStatus status) override {
    status_ = base::Optional<AlsReader::AlsInitStatus>(status);
  }

  int get_ambient_light() const { return ambient_light_; }
  int get_num_received_ambient_lights() const {
    return num_received_ambient_lights_;
  }

  AlsReader::AlsInitStatus get_status() const {
    CHECK(status_);
    return status_.value();
  }

 private:
  int ambient_light_ = -1;
  int num_received_ambient_lights_ = 0;
  base::Optional<AlsReader::AlsInitStatus> status_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};
}  // namespace

class FakeAlsReaderTest : public testing::Test {
 public:
  FakeAlsReaderTest() { fake_als_reader_.AddObserver(&test_observer_); }

  ~FakeAlsReaderTest() override {
    fake_als_reader_.RemoveObserver(&test_observer_);
  };

 protected:
  TestObserver test_observer_;
  FakeAlsReader fake_als_reader_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAlsReaderTest);
};

TEST_F(FakeAlsReaderTest, GetInitStatus) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kDisabled);
  EXPECT_EQ(AlsReader::AlsInitStatus::kDisabled,
            fake_als_reader_.GetInitStatus());
}

TEST_F(FakeAlsReaderTest, ReportReaderInitialized) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_als_reader_.ReportReaderInitialized();
  EXPECT_EQ(AlsReader::AlsInitStatus::kSuccess, test_observer_.get_status());
}

TEST_F(FakeAlsReaderTest, ReportAmbientLightUpdate) {
  fake_als_reader_.ReportAmbientLightUpdate(10);
  EXPECT_EQ(10, test_observer_.get_ambient_light());
  EXPECT_EQ(1, test_observer_.get_num_received_ambient_lights());
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
