// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber_service.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_io_thread_state.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_use_measurement {

class ChromeDataUseAscriberServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    prefs_ = base::MakeUnique<TestingPrefServiceSimple>();
    chrome::RegisterLocalState(prefs_->registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(prefs_.get());

    testing_io_thread_state_ = base::MakeUnique<chrome::TestingIOThreadState>();
    testing_io_thread_state_->io_thread_state()
        ->globals()
        ->data_use_ascriber.reset(
            new data_use_measurement::ChromeDataUseAscriber());
    service_ = base::MakeUnique<ChromeDataUseAscriberService>();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    testing_io_thread_state_.reset();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  ChromeDataUseAscriber* ascriber() { return service_->ascriber_; }

 private:
  // |thread_bundle_| must be the first field to ensure that threads are
  // constructed first and destroyed last.
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<ChromeDataUseAscriberService> service_;
  std::unique_ptr<chrome::TestingIOThreadState> testing_io_thread_state_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

TEST_F(ChromeDataUseAscriberServiceTest, Initialization) {
  EXPECT_NE(nullptr, ascriber());
}

}  // namespace data_use_measurement
