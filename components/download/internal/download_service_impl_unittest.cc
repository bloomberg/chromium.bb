// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/download_service_impl.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "components/download/internal/startup_status.h"
#include "components/download/internal/test/download_params_utils.h"
#include "components/download/internal/test/mock_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace download {
namespace {

class DownloadServiceImplTest : public testing::Test {
 public:
  DownloadServiceImplTest() : controller_(nullptr) {}
  ~DownloadServiceImplTest() override = default;

  void SetUp() override {
    auto config = base::MakeUnique<Configuration>();
    auto controller = base::MakeUnique<test::MockController>();
    controller_ = controller.get();

    EXPECT_CALL(*controller_, Initialize()).Times(1);

    service_ = base::MakeUnique<DownloadServiceImpl>(std::move(config),
                                                     std::move(controller));
  }

 protected:
  test::MockController* controller_;
  std::unique_ptr<DownloadServiceImpl> service_;

  DISALLOW_COPY_AND_ASSIGN(DownloadServiceImplTest);
};

}  // namespace

TEST_F(DownloadServiceImplTest, TestGetStatus) {
  StartupStatus startup_status;
  EXPECT_CALL(*controller_, GetStartupStatus())
      .WillRepeatedly(Return(&startup_status));

  EXPECT_EQ(DownloadService::ServiceStatus::STARTING_UP, service_->GetStatus());

  startup_status.driver_ok = true;
  startup_status.model_ok = true;
  startup_status.file_monitor_ok = true;
  EXPECT_EQ(DownloadService::ServiceStatus::READY, service_->GetStatus());

  startup_status.driver_ok = false;
  startup_status.model_ok = true;
  EXPECT_EQ(DownloadService::ServiceStatus::UNAVAILABLE, service_->GetStatus());
}

TEST_F(DownloadServiceImplTest, TestApiPassThrough) {
  DownloadParams params = test::BuildBasicDownloadParams();
  // TODO(xingliu): Remove the limitation of upper case guid in
  // |download_params|, see http://crbug.com/734818.
  params.guid = base::ToUpperASCII(params.guid);
  SchedulingParams scheduling_params;
  scheduling_params.priority = SchedulingParams::Priority::UI;

  EXPECT_CALL(*controller_, GetOwnerOfDownload(_))
      .WillRepeatedly(Return(DownloadClient::TEST));

  testing::Sequence seq;
  EXPECT_CALL(*controller_, StartDownload(_)).Times(1).InSequence(seq);
  EXPECT_CALL(*controller_, PauseDownload(params.guid))
      .Times(1)
      .InSequence(seq);
  EXPECT_CALL(*controller_, ResumeDownload(params.guid))
      .Times(1)
      .InSequence(seq);
  EXPECT_CALL(*controller_, CancelDownload(params.guid))
      .Times(1)
      .InSequence(seq);
  EXPECT_CALL(*controller_, ChangeDownloadCriteria(params.guid, _))
      .Times(1)
      .InSequence(seq);

  service_->StartDownload(params);
  service_->PauseDownload(params.guid);
  service_->ResumeDownload(params.guid);
  service_->CancelDownload(params.guid);
  service_->ChangeDownloadCriteria(params.guid, scheduling_params);
}

}  // namespace download
