// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_TEST_MOCK_CONTROLLER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_TEST_MOCK_CONTROLLER_H_

#include "base/macros.h"
#include "components/download/internal/controller.h"
#include "components/download/internal/startup_status.h"
#include "components/download/public/download_params.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace download {
namespace test {

class MockController : public Controller {
 public:
  MockController();
  ~MockController() override;

  // Controller implementation.
  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD0(GetStartupStatus, const StartupStatus*());
  MOCK_METHOD1(StartDownload, void(const DownloadParams&));
  MOCK_METHOD1(PauseDownload, void(const std::string&));
  MOCK_METHOD1(ResumeDownload, void(const std::string&));
  MOCK_METHOD1(CancelDownload, void(const std::string&));
  MOCK_METHOD2(ChangeDownloadCriteria,
               void(const std::string&, const SchedulingParams&));
  MOCK_METHOD1(GetOwnerOfDownload, DownloadClient(const std::string&));
  MOCK_METHOD2(OnStartScheduledTask,
               void(DownloadTaskType, const TaskFinishedCallback&));
  MOCK_METHOD1(OnStopScheduledTask, bool(DownloadTaskType task_type));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockController);
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_TEST_MOCK_CONTROLLER_H_
