// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/test/task_environment.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/application_controller_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;

namespace {

class MockFrame : public fuchsia::web::testing::Frame_TestBase {
 public:
  void NotImplemented_(const std::string& name) final {
    LOG(FATAL) << "No mock defined for " << name;
  }

  MOCK_METHOD(void,
              ConfigureInputTypes,
              (fuchsia::web::InputTypes types,
               fuchsia::web::AllowInputState allow));

  MOCK_METHOD(void,
              GetPrivateMemorySize,
              (GetPrivateMemorySizeCallback callback));
};

class ApplicationControllerImplTest : public chromium::cast::ApplicationContext,
                                      public testing::Test {
 public:
  ApplicationControllerImplTest()
      : application_context_(this),
        application_(&frame_, application_context_.NewBinding()) {
    base::RunLoop run_loop;
    wait_for_controller_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  ~ApplicationControllerImplTest() override = default;

 protected:
  // chromium::cast::ApplicationReceiver implementation.
  void GetMediaSessionId(GetMediaSessionIdCallback callback) final {
    NOTREACHED();
  }
  void SetApplicationController(
      fidl::InterfaceHandle<chromium::cast::ApplicationController> application)
      final {
    DCHECK(wait_for_controller_callback_);

    application_ptr_ = application.Bind();
    std::move(wait_for_controller_callback_).Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  MockFrame frame_;
  fidl::Binding<chromium::cast::ApplicationContext> application_context_;

  chromium::cast::ApplicationControllerPtr application_ptr_;
  ApplicationControllerImpl application_;
  base::OnceClosure wait_for_controller_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ApplicationControllerImplTest);
};

// Verifies that SetTouchInputEnabled() calls the Frame API correctly.
TEST_F(ApplicationControllerImplTest, ConfigureInputTypes) {
  base::RunLoop run_loop;

  EXPECT_CALL(frame_,
              ConfigureInputTypes(fuchsia::web::InputTypes::GESTURE_TAP |
                                      fuchsia::web::InputTypes::GESTURE_DRAG,
                                  fuchsia::web::AllowInputState::ALLOW))
      .Times(2);
  EXPECT_CALL(frame_,
              ConfigureInputTypes(fuchsia::web::InputTypes::GESTURE_TAP |
                                      fuchsia::web::InputTypes::GESTURE_DRAG,
                                  fuchsia::web::AllowInputState::DENY))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  application_ptr_->SetTouchInputEnabled(true);
  application_ptr_->SetTouchInputEnabled(true);
  application_ptr_->SetTouchInputEnabled(false);
  run_loop.Run();
}

// Verifies that SetTouchInputEnabled() calls the Frame API correctly.
TEST_F(ApplicationControllerImplTest, GetPrivateMemorySize) {
  constexpr uint64_t kMockSize = 12345;

  EXPECT_CALL(frame_, GetPrivateMemorySize(testing::_))
      .WillOnce(
          [](chromium::cast::ApplicationController::GetPrivateMemorySizeCallback
                 callback) { callback(kMockSize); });

  base::RunLoop run_loop;
  cr_fuchsia::ResultReceiver<uint64_t> result(run_loop.QuitClosure());
  application_ptr_->GetPrivateMemorySize(
      cr_fuchsia::CallbackToFitFunction(result.GetReceiveCallback()));
  run_loop.Run();

  EXPECT_EQ(*result, kMockSize);
}

}  // namespace
