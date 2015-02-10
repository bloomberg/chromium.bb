// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/presentation/presentation_service_impl.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;

namespace content {

class MockPresentationServiceDelegate : public PresentationServiceDelegate {
 public:
  MOCK_METHOD1(AddObserver,
      void(PresentationServiceDelegate::Observer* observer));
  MOCK_METHOD1(RemoveObserver,
      void(PresentationServiceDelegate::Observer* observer));
  MOCK_METHOD3(AddScreenAvailabilityListener,
      bool(
          int render_process_id,
          int routing_id,
          PresentationScreenAvailabilityListener* listener));
  MOCK_METHOD3(RemoveScreenAvailabilityListener,
      void(
          int render_process_id,
          int routing_id,
          PresentationScreenAvailabilityListener* listener));
  MOCK_METHOD2(RemoveAllScreenAvailabilityListeners,
      void(
          int render_process_id,
          int routing_id));
};

class PresentationServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  PresentationServiceImplTest() : callback_count_(0) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    EXPECT_CALL(mock_delegate_, AddObserver(_)).Times(1);
    service_impl_.reset(mojo::WeakBindToProxy(
        new PresentationServiceImpl(
            contents()->GetMainFrame(), contents(), &mock_delegate_),
        &service_ptr_));
  }

  void TearDown() override {
    service_ptr_.reset();

    EXPECT_CALL(mock_delegate_, RemoveObserver(Eq(service_impl_.get())))
        .Times(1);
    service_impl_.reset();

    RenderViewHostImplTestHarness::TearDown();
  }

  void GetScreenAvailabilityAndWait(
      const std::string& presentation_url,
      const base::Callback<void(bool)>& callback,
      bool delegate_success) {
    VLOG(1) << "GetScreenAvailabilityAndWait for " << presentation_url;
    base::RunLoop run_loop;
    // This will call to |service_impl_| via mojo. Process the message
    // using RunLoop.
    // The callback shouldn't be invoked since there is no availability
    // result yet.
    EXPECT_CALL(mock_delegate_, AddScreenAvailabilityListener(_, _, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            Return(delegate_success)));
    service_ptr_->GetScreenAvailability(presentation_url, callback);
    run_loop.Run();

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  }

  void ExpectListenerDoesNotExist(const std::string& presentation_url) {
    const auto& contexts = service_impl_->availability_contexts_;
    auto it = contexts.find(presentation_url);
    EXPECT_TRUE(it == contexts.end());
  }

  void RunLoopFor(base::TimeDelta duration) {
    base::RunLoop run_loop;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), duration);
    run_loop.Run();
  }

  void SaveQuitClosureAndRunLoop() {
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    run_loop_quit_closure_.Reset();
  }

  void ShouldNotBeCalled(bool available) {
    FAIL() << "Callback unexpectedly invoked with available = " << available;
  }

  void SimulateScreenAvailabilityChange(
      const std::string& presentation_url, bool available) {
    const auto& contexts = service_impl_->availability_contexts_;
    auto it = contexts.find(presentation_url);
    ASSERT_TRUE(it != contexts.end());
    it->second->OnScreenAvailabilityChanged(available);
  }

  void ScreenAvailabilityChangedCallback(bool expected, bool available) {
    ++callback_count_;
    EXPECT_EQ(expected, available);
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  MockPresentationServiceDelegate mock_delegate_;
  scoped_ptr<PresentationServiceImpl> service_impl_;
  mojo::InterfacePtr<presentation::PresentationService> service_ptr_;
  base::Closure run_loop_quit_closure_;
  int callback_count_;
};

TEST_F(PresentationServiceImplTest, GetScreenAvailability) {
  std::string presentation_url("http://fooUrl");
  GetScreenAvailabilityAndWait(
      presentation_url,
      base::Bind(
          &PresentationServiceImplTest::ScreenAvailabilityChangedCallback,
          base::Unretained(this), true),
      true);

  // Different presentation URL.
  GetScreenAvailabilityAndWait(
      "http://barUrl",
      base::Bind(&PresentationServiceImplTest::ShouldNotBeCalled,
          base::Unretained(this)),
      true);

  // Result now available; callback will be invoked with availability result.
  SimulateScreenAvailabilityChange(presentation_url, true);
  SaveQuitClosureAndRunLoop();

  EXPECT_EQ(1, callback_count_);

  // Result updated but callback not invoked since it's been erased.
  SimulateScreenAvailabilityChange(presentation_url, false);

  // Register another callback which should immediately invoke callback
  // since updated result is available.
  service_ptr_->GetScreenAvailability(
      presentation_url,
      base::Bind(
          &PresentationServiceImplTest::ScreenAvailabilityChangedCallback,
          base::Unretained(this),
          false));
  SaveQuitClosureAndRunLoop();
  EXPECT_EQ(2, callback_count_);
}

TEST_F(PresentationServiceImplTest, RemoveAllListeners) {
  std::string presentation_url("http://fooUrl");
  GetScreenAvailabilityAndWait(
      presentation_url,
      base::Bind(&PresentationServiceImplTest::ShouldNotBeCalled,
          base::Unretained(this)),
      true);

  service_impl_->RemoveAllListeners();

  ExpectListenerDoesNotExist(presentation_url);

  EXPECT_EQ(0, callback_count_);
}

TEST_F(PresentationServiceImplTest, DidNavigateThisFrame) {
  std::string presentation_url("http://fooUrl");
  GetScreenAvailabilityAndWait(
      presentation_url,
      base::Bind(&PresentationServiceImplTest::ShouldNotBeCalled,
          base::Unretained(this)),
      true);

  service_impl_->DidNavigateAnyFrame(
      contents()->GetMainFrame(),
      content::LoadCommittedDetails(),
      content::FrameNavigateParams());

  ExpectListenerDoesNotExist(presentation_url);
}

TEST_F(PresentationServiceImplTest, DidNavigateNotThisFrame) {
  std::string presentation_url("http://fooUrl");
  GetScreenAvailabilityAndWait(
      presentation_url,
      base::Bind(
          &PresentationServiceImplTest::ScreenAvailabilityChangedCallback,
          base::Unretained(this),
          true),
      true);

  // TODO(imcheng): How to get a different RenderFrameHost?
  service_impl_->DidNavigateAnyFrame(
      nullptr,
      content::LoadCommittedDetails(),
      content::FrameNavigateParams());

  // Availability is reported and callback is invoked since it was not
  // removed.
  SimulateScreenAvailabilityChange(presentation_url, true);
  SaveQuitClosureAndRunLoop();
  EXPECT_EQ(1, callback_count_);
}

TEST_F(PresentationServiceImplTest, ThisRenderFrameDeleted) {
  std::string presentation_url("http://fooUrl");
  GetScreenAvailabilityAndWait(
      presentation_url,
      base::Bind(&PresentationServiceImplTest::ShouldNotBeCalled,
          base::Unretained(this)),
      true);

  service_impl_->RenderFrameDeleted(contents()->GetMainFrame());

  ExpectListenerDoesNotExist(presentation_url);
}

TEST_F(PresentationServiceImplTest, NotThisRenderFrameDeleted) {
    std::string presentation_url("http://fooUrl");
  GetScreenAvailabilityAndWait(
      presentation_url,
      base::Bind(
          &PresentationServiceImplTest::ScreenAvailabilityChangedCallback,
          base::Unretained(this),
          true),
      true);

  // TODO(imcheng): How to get a different RenderFrameHost?
  service_impl_->RenderFrameDeleted(nullptr);

  // Availability is reported and callback should be invoked since listener
  // has not been deleted.
  SimulateScreenAvailabilityChange(presentation_url, true);
  SaveQuitClosureAndRunLoop();
  EXPECT_EQ(1, callback_count_);
}

TEST_F(PresentationServiceImplTest, GetScreenAvailabilityTwice) {
  std::string presentation_url("http://fooUrl");
  GetScreenAvailabilityAndWait(
      presentation_url,
      base::Bind(&PresentationServiceImplTest::ShouldNotBeCalled,
          base::Unretained(this)),
      true);

  // Second call should overwrite the callback from first call.
  // It shouldn't result in an extra call to delegate.
  service_ptr_->GetScreenAvailability(
      presentation_url,
      base::Bind(
          &PresentationServiceImplTest::ScreenAvailabilityChangedCallback,
          base::Unretained(this),
          false));

  // Cannot use GetScreenAvailabilityAndWait here since the mock delegate
  // won't be triggered again to quit the RunLoop.
  RunLoopFor(base::TimeDelta::FromMilliseconds(50));

  // Result now available; callback will be invoked with availability result.
  SimulateScreenAvailabilityChange(presentation_url, false);
  SaveQuitClosureAndRunLoop();

  EXPECT_EQ(1, callback_count_);
}

TEST_F(PresentationServiceImplTest, DelegateFails) {
  std::string presentation_url("http://fooUrl");
  GetScreenAvailabilityAndWait(
      presentation_url,
      base::Bind(&PresentationServiceImplTest::ShouldNotBeCalled,
          base::Unretained(this)),
      false);

  ExpectListenerDoesNotExist(presentation_url);
}

}  // namespace content
