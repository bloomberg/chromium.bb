// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_io_data.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "components/previews/core/previews_ui_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

class TestPreviewsIOData : public PreviewsIOData {
 public:
  TestPreviewsIOData(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : PreviewsIOData(io_task_runner, ui_task_runner), initialized_(false) {}
  ~TestPreviewsIOData() override {}

  // Whether Initialize was called.
  bool initialized() { return initialized_; }

 private:
  // Set |initialized_| to true and use base class functionality.
  void InitializeOnIOThread(
      std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store) override {
    initialized_ = true;
    PreviewsIOData::InitializeOnIOThread(std::move(previews_opt_out_store));
  }

  // Whether Initialize was called.
  bool initialized_;
};

class PreviewsIODataTest : public testing::Test {
 public:
  PreviewsIODataTest() {}

  ~PreviewsIODataTest() override {}

  void set_io_data(std::unique_ptr<TestPreviewsIOData> io_data) {
    io_data_ = std::move(io_data);
  }

  TestPreviewsIOData* io_data() { return io_data_.get(); }

  void set_ui_service(std::unique_ptr<PreviewsUIService> ui_service) {
    ui_service_ = std::move(ui_service);
  }

 protected:
  // Run this test on a single thread.
  base::MessageLoopForIO loop_;

 private:
  std::unique_ptr<TestPreviewsIOData> io_data_;
  std::unique_ptr<PreviewsUIService> ui_service_;
};

}  // namespace

TEST_F(PreviewsIODataTest, TestInitialization) {
  set_io_data(base::WrapUnique(
      new TestPreviewsIOData(loop_.task_runner(), loop_.task_runner())));
  set_ui_service(base::WrapUnique(
      new PreviewsUIService(io_data(), loop_.task_runner(), nullptr)));
  base::RunLoop().RunUntilIdle();
  // After the outstanding posted tasks have run, |io_data_| should be fully
  // initialized.
  EXPECT_TRUE(io_data()->initialized());
}

}  // namespace previews
