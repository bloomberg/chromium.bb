// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/presentation/presentation_service_impl.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/presentation_session.h"
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
using ::testing::SaveArg;

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
  MOCK_METHOD2(Reset,
      void(
          int render_process_id,
          int routing_id));
  MOCK_METHOD4(SetDefaultPresentationUrl,
      void(
          int render_process_id,
          int routing_id,
          const std::string& default_presentation_url,
          const std::string& default_presentation_id));
  MOCK_METHOD6(StartSession,
      void(
          int render_process_id,
          int render_frame_id,
          const std::string& presentation_url,
          const std::string& presentation_id,
          const PresentationSessionSuccessCallback& success_cb,
          const PresentationSessionErrorCallback& error_cb));
  MOCK_METHOD6(JoinSession,
      void(
          int render_process_id,
          int render_frame_id,
          const std::string& presentation_url,
          const std::string& presentation_id,
          const PresentationSessionSuccessCallback& success_cb,
          const PresentationSessionErrorCallback& error_cb));
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
      const base::Callback<void(const std::string&, bool)>& callback,
      bool delegate_success) {
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

  void ShouldNotBeCalled(const std::string& presentation_url, bool available) {
    FAIL() << "Callback unexpectedly invoked with "
           << "url = " << presentation_url << ", available = " << available;
  }

  void SimulateScreenAvailabilityChange(
      const std::string& presentation_url, bool available) {
    const auto& contexts = service_impl_->availability_contexts_;
    auto it = contexts.find(presentation_url);
    ASSERT_TRUE(it != contexts.end());
    it->second->OnScreenAvailabilityChanged(available);
  }

  void ScreenAvailabilityChangedCallback(
      bool expected,
      const std::string& presentation_url,
      bool available) {
    ++callback_count_;
    EXPECT_EQ(expected, available);
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectReset() {
    EXPECT_CALL(mock_delegate_, Reset(_, _))
        .Times(1);
  }

  void ExpectCleanState() {
    EXPECT_TRUE(service_impl_->availability_contexts_.empty());
    EXPECT_TRUE(service_impl_->default_presentation_url_.empty());
    EXPECT_TRUE(service_impl_->default_presentation_id_.empty());
    EXPECT_TRUE(service_impl_->queued_start_session_requests_.empty());
  }

  void ExpectNewSessionMojoCallbackSuccess(
      presentation::PresentationSessionInfoPtr info,
      presentation::PresentationErrorPtr error) {
    EXPECT_FALSE(info.is_null());
    EXPECT_TRUE(error.is_null());
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectNewSessionMojoCallbackError(
      presentation::PresentationSessionInfoPtr info,
      presentation::PresentationErrorPtr error) {
    EXPECT_TRUE(info.is_null());
    EXPECT_FALSE(error.is_null());
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

TEST_F(PresentationServiceImplTest, Reset) {
  std::string presentation_url("http://fooUrl");
  GetScreenAvailabilityAndWait(
      presentation_url,
      base::Bind(&PresentationServiceImplTest::ShouldNotBeCalled,
          base::Unretained(this)),
      true);

  ExpectReset();
  service_impl_->Reset();
  ExpectCleanState();

  EXPECT_EQ(0, callback_count_);
}

TEST_F(PresentationServiceImplTest, DidNavigateThisFrame) {
  std::string presentation_url("http://fooUrl");
  GetScreenAvailabilityAndWait(
      presentation_url,
      base::Bind(&PresentationServiceImplTest::ShouldNotBeCalled,
          base::Unretained(this)),
      true);

  ExpectReset();
  service_impl_->DidNavigateAnyFrame(
      contents()->GetMainFrame(),
      content::LoadCommittedDetails(),
      content::FrameNavigateParams());
  ExpectCleanState();
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

  ExpectReset();
  service_impl_->RenderFrameDeleted(contents()->GetMainFrame());
  ExpectCleanState();
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
      base::Bind(
          &PresentationServiceImplTest::ScreenAvailabilityChangedCallback,
          base::Unretained(this),
          false),
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

  EXPECT_EQ(2, callback_count_);
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

TEST_F(PresentationServiceImplTest, SetDefaultPresentationUrl) {
  std::string url1("http://fooUrl");
  std::string dpu_id("dpuId");
  EXPECT_CALL(mock_delegate_,
      SetDefaultPresentationUrl(_, _, Eq(url1), Eq(dpu_id)))
      .Times(1);
  service_impl_->SetDefaultPresentationURL(url1, dpu_id);
  EXPECT_EQ(url1, service_impl_->default_presentation_url_);

  // Now there should be a callback registered with the DPU.
  GetScreenAvailabilityAndWait(
      url1,
      base::Bind(
          &PresentationServiceImplTest::ScreenAvailabilityChangedCallback,
          base::Unretained(this), true),
      true);
  const auto& contexts = service_impl_->availability_contexts_;
  auto it = contexts.find(url1);
  ASSERT_TRUE(it != contexts.end());
  EXPECT_TRUE(it->second->HasPendingCallbacks());

  std::string url2("http://barUrl");
  // Sets different DPU.
  // Adds listener for url2 and removes listener for url1.
  // Also, the callback from url1 is transferred to url2.
  EXPECT_CALL(
      mock_delegate_,
      AddScreenAvailabilityListener(_, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(
      mock_delegate_,
      RemoveScreenAvailabilityListener(_, _, _))
      .Times(1);
  EXPECT_CALL(mock_delegate_,
      SetDefaultPresentationUrl(_, _, Eq(url2), Eq(dpu_id)))
      .Times(1);
  service_impl_->SetDefaultPresentationURL(url2, dpu_id);
  EXPECT_EQ(url2, service_impl_->default_presentation_url_);

  it = contexts.find(url2);
  ASSERT_TRUE(it != contexts.end());
  EXPECT_TRUE(it->second->HasPendingCallbacks());
}

TEST_F(PresentationServiceImplTest, SetSameDefaultPresentationUrl) {
  std::string url("http://fooUrl");
  std::string dpu_id("dpuId");
  EXPECT_CALL(mock_delegate_,
      SetDefaultPresentationUrl(_, _, Eq(url), Eq(dpu_id)))
      .Times(1);
  service_impl_->SetDefaultPresentationURL(url, dpu_id);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  EXPECT_EQ(url, service_impl_->default_presentation_url_);

  // Same URL as before; no-ops.
  service_impl_->SetDefaultPresentationURL(url, dpu_id);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  EXPECT_EQ(url, service_impl_->default_presentation_url_);
}

TEST_F(PresentationServiceImplTest, ClearDefaultPresentationUrl) {
  std::string url("http://fooUrl");
  std::string dpu_id("dpuId");
  EXPECT_CALL(mock_delegate_,
      SetDefaultPresentationUrl(_, _, Eq(url), Eq(dpu_id)))
      .Times(1);
  service_impl_->SetDefaultPresentationURL(url, dpu_id);
  EXPECT_EQ(url, service_impl_->default_presentation_url_);

  // Now there should be a callback registered with the DPU.
  GetScreenAvailabilityAndWait(
      url,
      base::Bind(
          &PresentationServiceImplTest::ScreenAvailabilityChangedCallback,
          base::Unretained(this), true),
      true);

  const auto& contexts = service_impl_->availability_contexts_;
  auto it = contexts.find(url);
  ASSERT_TRUE(it != contexts.end());
  EXPECT_TRUE(it->second->HasPendingCallbacks());

  // Clears the default presentation URL. Transfers the listener from url to
  // "1-UA" mode.
  EXPECT_CALL(
      mock_delegate_,
      AddScreenAvailabilityListener(_, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(
      mock_delegate_,
      RemoveScreenAvailabilityListener(_, _, _))
      .Times(1);
  EXPECT_CALL(
      mock_delegate_,
      SetDefaultPresentationUrl(_, _, Eq(std::string()), Eq(std::string())))
      .Times(1);
  service_impl_->SetDefaultPresentationURL(std::string(), std::string());
  EXPECT_TRUE(service_impl_->default_presentation_url_.empty());

  it = contexts.find(url);
  ASSERT_TRUE(it == contexts.end());
}

TEST_F(PresentationServiceImplTest, StartSessionSuccess) {
  std::string presentation_url("http://fooUrl");
  std::string presentation_id("presentationId");
  service_ptr_->StartSession(
      presentation_url,
      presentation_id,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionMojoCallbackSuccess,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationSessionInfo&)> success_cb;
  EXPECT_CALL(mock_delegate_, StartSession(
      _, _, Eq(presentation_url), Eq(presentation_id), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<4>(&success_cb)));
  run_loop.Run();
  success_cb.Run(PresentationSessionInfo(presentation_url, presentation_id));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, StartSessionError) {
  std::string presentation_url("http://fooUrl");
  std::string presentation_id("presentationId");
  service_ptr_->StartSession(
      presentation_url,
      presentation_id,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionMojoCallbackError,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationError&)> error_cb;
  EXPECT_CALL(mock_delegate_, StartSession(
      _, _, Eq(presentation_url), Eq(presentation_id), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<5>(&error_cb)));
  run_loop.Run();
  error_cb.Run(PresentationError(PRESENTATION_ERROR_UNKNOWN, "Error message"));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, JoinSessionSuccess) {
  std::string presentation_url("http://fooUrl");
  std::string presentation_id("presentationId");
  service_ptr_->JoinSession(
      presentation_url,
      presentation_id,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionMojoCallbackSuccess,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationSessionInfo&)> success_cb;
  EXPECT_CALL(mock_delegate_, JoinSession(
      _, _, Eq(presentation_url), Eq(presentation_id), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<4>(&success_cb)));
  run_loop.Run();
  success_cb.Run(PresentationSessionInfo(presentation_url, presentation_id));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, JoinSessionError) {
  std::string presentation_url("http://fooUrl");
  std::string presentation_id("presentationId");
  service_ptr_->JoinSession(
      presentation_url,
      presentation_id,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionMojoCallbackError,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationError&)> error_cb;
  EXPECT_CALL(mock_delegate_, JoinSession(
      _, _, Eq(presentation_url), Eq(presentation_id), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<5>(&error_cb)));
  run_loop.Run();
  error_cb.Run(PresentationError(PRESENTATION_ERROR_UNKNOWN, "Error message"));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, StartSessionInProgress) {
  std::string presentation_url1("http://fooUrl");
  std::string presentation_id1("presentationId1");
  std::string presentation_url2("http://barUrl");
  std::string presentation_id2("presentationId2");
  service_ptr_->StartSession(
      presentation_url1,
      presentation_id1,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionMojoCallbackSuccess,
          base::Unretained(this)));
  service_ptr_->StartSession(
      presentation_url2,
      presentation_id2,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionMojoCallbackSuccess,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationSessionInfo&)> success_cb;
  EXPECT_CALL(mock_delegate_, StartSession(
      _, _, Eq(presentation_url1), Eq(presentation_id1), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<4>(&success_cb)));
  run_loop.Run();

  // Running the callback means the first request is done. It should now
  // move on to the queued request.
  EXPECT_CALL(mock_delegate_, StartSession(
      _, _, Eq(presentation_url2), Eq(presentation_id2), _, _))
      .Times(1);
  success_cb.Run(PresentationSessionInfo(presentation_url1, presentation_id1));
  SaveQuitClosureAndRunLoop();
}

}  // namespace content
