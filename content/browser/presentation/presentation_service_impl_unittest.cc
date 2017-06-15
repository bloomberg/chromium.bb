// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_service_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/common/presentation_connection_message.h"
#include "content/public/common/presentation_info.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArgs;

namespace content {

namespace {

// Matches content::PresentationInfo.
MATCHER_P(InfoEquals, expected, "") {
  return expected.presentation_url == arg.presentation_url &&
         expected.presentation_id == arg.presentation_id;
}

const char kPresentationId[] = "presentationId";
const char kPresentationUrl1[] = "http://foo.com/index.html";
const char kPresentationUrl2[] = "http://example.com/index.html";
const char kPresentationUrl3[] = "http://example.net/index.html";

void DoNothing(const base::Optional<content::PresentationInfo>& info,
               const base::Optional<content::PresentationError>& error) {}

}  // namespace

class MockPresentationServiceDelegate
    : public ControllerPresentationServiceDelegate {
 public:
  MOCK_METHOD3(AddObserver,
               void(int render_process_id,
                    int render_frame_id,
                    PresentationServiceDelegate::Observer* observer));
  MOCK_METHOD2(RemoveObserver,
      void(int render_process_id, int render_frame_id));

  bool AddScreenAvailabilityListener(
      int render_process_id,
      int routing_id,
      PresentationScreenAvailabilityListener* listener) override {
    if (!screen_availability_listening_supported_)
      listener->OnScreenAvailabilityNotSupported();

    return AddScreenAvailabilityListener();
  }
  MOCK_METHOD0(AddScreenAvailabilityListener, bool());

  MOCK_METHOD3(RemoveScreenAvailabilityListener,
      void(int render_process_id,
           int routing_id,
           PresentationScreenAvailabilityListener* listener));
  MOCK_METHOD2(Reset,
      void(int render_process_id,
           int routing_id));
  MOCK_METHOD4(SetDefaultPresentationUrls,
               void(int render_process_id,
                    int routing_id,
                    const std::vector<GURL>& default_presentation_urls,
                    const PresentationConnectionCallback& callback));
  MOCK_METHOD5(StartPresentation,
               void(int render_process_id,
                    int render_frame_id,
                    const std::vector<GURL>& presentation_urls,
                    const PresentationConnectionCallback& success_cb,
                    const PresentationConnectionErrorCallback& error_cb));
  MOCK_METHOD6(ReconnectPresentation,
               void(int render_process_id,
                    int render_frame_id,
                    const std::vector<GURL>& presentation_urls,
                    const std::string& presentation_id,
                    const PresentationConnectionCallback& success_cb,
                    const PresentationConnectionErrorCallback& error_cb));
  MOCK_METHOD3(CloseConnection,
               void(int render_process_id,
                    int render_frame_id,
                    const std::string& presentation_id));
  MOCK_METHOD3(Terminate,
               void(int render_process_id,
                    int render_frame_id,
                    const std::string& presentation_id));
  MOCK_METHOD4(ListenForConnectionMessages,
               void(int render_process_id,
                    int render_frame_id,
                    const PresentationInfo& presentation_info,
                    const PresentationConnectionMessageCallback& message_cb));

  // PresentationConnectionMessage is move-only.
  // TODO(crbug.com/729950): Use MOCK_METHOD directly once GMock gets the
  // move-only type support.
  void SendMessage(int render_process_id,
                   int render_frame_id,
                   const PresentationInfo& presentation_info,
                   PresentationConnectionMessage message,
                   SendMessageCallback send_message_cb) {
    SendMessageInternal(render_process_id, render_frame_id, presentation_info,
                        message, send_message_cb);
  }
  MOCK_METHOD5(SendMessageInternal,
               void(int render_process_id,
                    int render_frame_id,
                    const PresentationInfo& presentation_info,
                    const PresentationConnectionMessage& message,
                    const SendMessageCallback& send_message_cb));

  MOCK_METHOD4(
      ListenForConnectionStateChange,
      void(int render_process_id,
           int render_frame_id,
           const PresentationInfo& connection,
           const PresentationConnectionStateChangedCallback& state_changed_cb));

  void ConnectToPresentation(
      int render_process_id,
      int render_frame_id,
      const PresentationInfo& presentation_info,
      PresentationConnectionPtr controller_conn_ptr,
      PresentationConnectionRequest receiver_conn_request) override {
    RegisterOffscreenPresentationConnectionRaw(
        render_process_id, render_frame_id, presentation_info,
        controller_conn_ptr.get());
  }

  MOCK_METHOD4(RegisterOffscreenPresentationConnectionRaw,
               void(int render_process_id,
                    int render_frame_id,
                    const PresentationInfo& presentation_info,
                    blink::mojom::PresentationConnection* connection));

  void set_screen_availability_listening_supported(bool value) {
    screen_availability_listening_supported_ = value;
  }

 private:
  bool screen_availability_listening_supported_ = true;
};

class MockReceiverPresentationServiceDelegate
    : public ReceiverPresentationServiceDelegate {
 public:
  MOCK_METHOD3(AddObserver,
               void(int render_process_id,
                    int render_frame_id,
                    PresentationServiceDelegate::Observer* observer));
  MOCK_METHOD2(RemoveObserver,
               void(int render_process_id, int render_frame_id));
  MOCK_METHOD2(Reset, void(int render_process_id, int routing_id));
  MOCK_METHOD1(RegisterReceiverConnectionAvailableCallback,
               void(const ReceiverConnectionAvailableCallback&));
};

class MockPresentationConnection : public blink::mojom::PresentationConnection {
 public:
  // PresentationConnectionMessage is move-only.
  void OnMessage(PresentationConnectionMessage message,
                 base::OnceCallback<void(bool)> send_message_cb) {
    OnMessageInternal(message, send_message_cb);
  }
  MOCK_METHOD2(OnMessageInternal,
               void(const PresentationConnectionMessage& message,
                    base::OnceCallback<void(bool)>& send_message_cb));
  MOCK_METHOD1(DidChangeState, void(PresentationConnectionState state));
  MOCK_METHOD0(OnClose, void());
};

class MockPresentationServiceClient
    : public blink::mojom::PresentationServiceClient {
 public:
  MOCK_METHOD2(OnScreenAvailabilityUpdated,
               void(const GURL& url,
                    blink::mojom::ScreenAvailability availability));
  MOCK_METHOD2(OnConnectionStateChanged,
               void(const PresentationInfo& connection,
                    PresentationConnectionState new_state));
  MOCK_METHOD3(OnConnectionClosed,
               void(const PresentationInfo& connection,
                    PresentationConnectionCloseReason reason,
                    const std::string& message));
  MOCK_METHOD1(OnScreenAvailabilityNotSupported, void(const GURL& url));
  // PresentationConnectionMessage is move-only.
  void OnConnectionMessagesReceived(
      const PresentationInfo& presentation_info,
      std::vector<PresentationConnectionMessage> messages) {
    OnConnectionMessagesReceivedInternal(presentation_info, messages);
  }
  MOCK_METHOD2(
      OnConnectionMessagesReceivedInternal,
      void(const PresentationInfo& presentation_info,
           const std::vector<PresentationConnectionMessage>& messages));
  MOCK_METHOD1(OnDefaultPresentationStarted,
               void(const PresentationInfo& presentation_info));

  void OnReceiverConnectionAvailable(
      const PresentationInfo& presentation_info,
      blink::mojom::PresentationConnectionPtr controller_conn_ptr,
      blink::mojom::PresentationConnectionRequest receiver_conn_request)
      override {
    OnReceiverConnectionAvailable(presentation_info);
  }
  MOCK_METHOD1(OnReceiverConnectionAvailable,
               void(const PresentationInfo& presentation_info));
};

class PresentationServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  PresentationServiceImplTest()
      : presentation_url1_(GURL(kPresentationUrl1)),
        presentation_url2_(GURL(kPresentationUrl2)),
        presentation_url3_(GURL(kPresentationUrl3)) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    // This needed to keep the WebContentsObserverSanityChecker checks happy for
    // when AppendChild is called.
    NavigateAndCommit(GURL("about:blank"));

    auto request = mojo::MakeRequest(&service_ptr_);
    EXPECT_CALL(mock_delegate_, AddObserver(_, _, _)).Times(1);
    TestRenderFrameHost* render_frame_host = contents()->GetMainFrame();
    render_frame_host->InitializeRenderFrameIfNeeded();
    service_impl_.reset(new PresentationServiceImpl(
        render_frame_host, contents(), &mock_delegate_, nullptr));
    service_impl_->Bind(std::move(request));

    blink::mojom::PresentationServiceClientPtr client_ptr;
    client_binding_.reset(
        new mojo::Binding<blink::mojom::PresentationServiceClient>(
            &mock_client_, mojo::MakeRequest(&client_ptr)));
    service_impl_->SetClient(std::move(client_ptr));

    presentation_urls_.push_back(presentation_url1_);
    presentation_urls_.push_back(presentation_url2_);
  }

  void TearDown() override {
    service_ptr_.reset();
    if (service_impl_.get()) {
      EXPECT_CALL(mock_delegate_, RemoveObserver(_, _)).Times(1);
      service_impl_.reset();
    }
    RenderViewHostImplTestHarness::TearDown();
  }

  void Navigate(bool main_frame) {
    RenderFrameHost* rfh = main_rfh();
    RenderFrameHostTester* rfh_tester = RenderFrameHostTester::For(rfh);
    if (!main_frame)
      rfh = rfh_tester->AppendChild("subframe");
    std::unique_ptr<NavigationHandle> navigation_handle =
        NavigationHandle::CreateNavigationHandleForTesting(
            GURL(), rfh, true);
    // Destructor calls DidFinishNavigation.
  }

  void ListenForScreenAvailabilityAndWait(const GURL& url,
                                          bool delegate_success) {
    base::RunLoop run_loop;
    // This will call to |service_impl_| via mojo. Process the message
    // using RunLoop.
    // The callback shouldn't be invoked since there is no availability
    // result yet.
    EXPECT_CALL(mock_delegate_, AddScreenAvailabilityListener())
        .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            Return(delegate_success)));
    service_ptr_->ListenForScreenAvailability(url);
    run_loop.Run();

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  }

  void RunLoopFor(base::TimeDelta duration) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), duration);
    run_loop.Run();
  }

  void SaveQuitClosureAndRunLoop() {
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    run_loop_quit_closure_.Reset();
  }

  void SimulateScreenAvailabilityChangeAndWait(const GURL& url,
                                               bool available) {
    auto listener_it = service_impl_->screen_availability_listeners_.find(url);
    ASSERT_TRUE(listener_it->second);

    base::RunLoop run_loop;
    blink::mojom::ScreenAvailability expected_availability =
        available ? blink::mojom::ScreenAvailability::AVAILABLE
                  : blink::mojom::ScreenAvailability::UNAVAILABLE;
    EXPECT_CALL(mock_client_,
                OnScreenAvailabilityUpdated(url, expected_availability))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    listener_it->second->OnScreenAvailabilityChanged(available);
    run_loop.Run();
  }

  void ExpectReset() {
    EXPECT_CALL(mock_delegate_, Reset(_, _)).Times(1);
  }

  void ExpectCleanState() {
    EXPECT_TRUE(service_impl_->default_presentation_urls_.empty());
    EXPECT_EQ(
        service_impl_->screen_availability_listeners_.find(presentation_url1_),
        service_impl_->screen_availability_listeners_.end());
  }

  void ExpectNewPresentationCallbackSuccess(
      const base::Optional<PresentationInfo>& info,
      const base::Optional<PresentationError>& error) {
    EXPECT_TRUE(info);
    EXPECT_FALSE(error);
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectNewPresentationCallbackError(
      const base::Optional<PresentationInfo>& info,
      const base::Optional<PresentationError>& error) {
    EXPECT_FALSE(info);
    EXPECT_TRUE(error);
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectConnectionMessages(
      const std::vector<PresentationConnectionMessage>& expected_msgs,
      const std::vector<PresentationConnectionMessage>& actual_msgs) {
    EXPECT_EQ(expected_msgs.size(), actual_msgs.size());
    for (size_t i = 0; i < actual_msgs.size(); ++i)
      EXPECT_EQ(expected_msgs[i], actual_msgs[i]);
  }

  MockPresentationServiceDelegate mock_delegate_;
  MockReceiverPresentationServiceDelegate mock_receiver_delegate_;

  std::unique_ptr<PresentationServiceImpl> service_impl_;
  mojo::InterfacePtr<blink::mojom::PresentationService> service_ptr_;

  MockPresentationServiceClient mock_client_;
  std::unique_ptr<mojo::Binding<blink::mojom::PresentationServiceClient>>
      client_binding_;

  base::Closure run_loop_quit_closure_;

  GURL presentation_url1_;
  GURL presentation_url2_;
  GURL presentation_url3_;
  std::vector<GURL> presentation_urls_;
};

TEST_F(PresentationServiceImplTest, ListenForScreenAvailability) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  SimulateScreenAvailabilityChangeAndWait(presentation_url1_, true);
  SimulateScreenAvailabilityChangeAndWait(presentation_url1_, false);
  SimulateScreenAvailabilityChangeAndWait(presentation_url1_, true);
}

TEST_F(PresentationServiceImplTest, Reset) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  ExpectReset();
  service_impl_->Reset();
  ExpectCleanState();
}

TEST_F(PresentationServiceImplTest, DidNavigateThisFrame) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  ExpectReset();
  Navigate(true);
  ExpectCleanState();
}

TEST_F(PresentationServiceImplTest, DidNavigateOtherFrame) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  Navigate(false);

  // Availability is reported and callback is invoked since it was not
  // removed.
  SimulateScreenAvailabilityChangeAndWait(presentation_url1_, true);
}

TEST_F(PresentationServiceImplTest, ThisRenderFrameDeleted) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  ExpectReset();

  // Since the frame matched the service, |service_impl_| will be deleted.
  PresentationServiceImpl* service = service_impl_.release();
  EXPECT_CALL(mock_delegate_, RemoveObserver(_, _)).Times(1);
  service->RenderFrameDeleted(contents()->GetMainFrame());
}

TEST_F(PresentationServiceImplTest, OtherRenderFrameDeleted) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  // TODO(imcheng): How to get a different RenderFrameHost?
  service_impl_->RenderFrameDeleted(nullptr);

  // Availability is reported and callback should be invoked since listener
  // has not been deleted.
  SimulateScreenAvailabilityChangeAndWait(presentation_url1_, true);
}

TEST_F(PresentationServiceImplTest, DelegateFails) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, false);
  ASSERT_EQ(
      service_impl_->screen_availability_listeners_.find(presentation_url1_),
      service_impl_->screen_availability_listeners_.end());
}

TEST_F(PresentationServiceImplTest, SetDefaultPresentationUrls) {
  EXPECT_CALL(mock_delegate_,
              SetDefaultPresentationUrls(_, _, presentation_urls_, _))
      .Times(1);

  service_impl_->SetDefaultPresentationUrls(presentation_urls_);

  // Sets different DPUs.
  std::vector<GURL> more_urls = presentation_urls_;
  more_urls.push_back(presentation_url3_);

  PresentationConnectionCallback callback;
  EXPECT_CALL(mock_delegate_, SetDefaultPresentationUrls(_, _, more_urls, _))
      .WillOnce(SaveArg<3>(&callback));
  service_impl_->SetDefaultPresentationUrls(more_urls);

  PresentationInfo presentation_info(presentation_url2_, kPresentationId);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_client_,
              OnDefaultPresentationStarted(InfoEquals(presentation_info)))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _));
  callback.Run(PresentationInfo(presentation_url2_, kPresentationId));
  run_loop.Run();
}

TEST_F(PresentationServiceImplTest, ListenForConnectionStateChange) {
  PresentationInfo connection(presentation_url1_, kPresentationId);
  PresentationConnectionStateChangedCallback state_changed_cb;
  // Trigger state change. It should be propagated back up to |mock_client_|.
  PresentationInfo presentation_connection(presentation_url1_, kPresentationId);

  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .WillOnce(SaveArg<3>(&state_changed_cb));
  service_impl_->ListenForConnectionStateChange(connection);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_client_, OnConnectionStateChanged(
                                  InfoEquals(presentation_connection),
                                  PRESENTATION_CONNECTION_STATE_TERMINATED))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    state_changed_cb.Run(PresentationConnectionStateChangeInfo(
        PRESENTATION_CONNECTION_STATE_TERMINATED));
    run_loop.Run();
  }
}

TEST_F(PresentationServiceImplTest, ListenForConnectionClose) {
  PresentationInfo connection(presentation_url1_, kPresentationId);
  PresentationConnectionStateChangedCallback state_changed_cb;
  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .WillOnce(SaveArg<3>(&state_changed_cb));
  service_impl_->ListenForConnectionStateChange(connection);

  // Trigger connection close. It should be propagated back up to
  // |mock_client_|.
  PresentationInfo presentation_connection(presentation_url1_, kPresentationId);
  {
    base::RunLoop run_loop;
    PresentationConnectionStateChangeInfo closed_info(
        PRESENTATION_CONNECTION_STATE_CLOSED);
    closed_info.close_reason = PRESENTATION_CONNECTION_CLOSE_REASON_WENT_AWAY;
    closed_info.message = "Foo";

    EXPECT_CALL(mock_client_,
                OnConnectionClosed(
                    InfoEquals(presentation_connection),
                    PRESENTATION_CONNECTION_CLOSE_REASON_WENT_AWAY, "Foo"))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    state_changed_cb.Run(closed_info);
    run_loop.Run();
  }
}

TEST_F(PresentationServiceImplTest, SetSameDefaultPresentationUrls) {
  EXPECT_CALL(mock_delegate_,
              SetDefaultPresentationUrls(_, _, presentation_urls_, _))
      .Times(1);
  service_impl_->SetDefaultPresentationUrls(presentation_urls_);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));

  // Same URLs as before; no-ops.
  service_impl_->SetDefaultPresentationUrls(presentation_urls_);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
}

TEST_F(PresentationServiceImplTest, StartPresentationSuccess) {
  service_ptr_->StartPresentation(
      presentation_urls_,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewPresentationCallbackSuccess,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationInfo&)> success_cb;
  EXPECT_CALL(mock_delegate_, StartPresentation(_, _, presentation_urls_, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<3>(&success_cb)));
  run_loop.Run();

  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .Times(1);
  success_cb.Run(PresentationInfo(presentation_url1_, kPresentationId));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, StartPresentationError) {
  service_ptr_->StartPresentation(
      presentation_urls_,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewPresentationCallbackError,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationError&)> error_cb;
  EXPECT_CALL(mock_delegate_, StartPresentation(_, _, presentation_urls_, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<4>(&error_cb)));
  run_loop.Run();
  error_cb.Run(PresentationError(PRESENTATION_ERROR_UNKNOWN, "Error message"));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, ReconnectPresentationSuccess) {
  service_ptr_->ReconnectPresentation(
      presentation_urls_, base::Optional<std::string>(kPresentationId),
      base::Bind(
          &PresentationServiceImplTest::ExpectNewPresentationCallbackSuccess,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationInfo&)> success_cb;
  EXPECT_CALL(mock_delegate_, ReconnectPresentation(_, _, presentation_urls_,
                                                    kPresentationId, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<4>(&success_cb)));
  run_loop.Run();

  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .Times(1);
  success_cb.Run(PresentationInfo(presentation_url1_, kPresentationId));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, ReconnectPresentationError) {
  service_ptr_->ReconnectPresentation(
      presentation_urls_, base::Optional<std::string>(kPresentationId),
      base::Bind(
          &PresentationServiceImplTest::ExpectNewPresentationCallbackError,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationError&)> error_cb;
  EXPECT_CALL(mock_delegate_, ReconnectPresentation(_, _, presentation_urls_,
                                                    kPresentationId, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<5>(&error_cb)));
  run_loop.Run();
  error_cb.Run(PresentationError(PRESENTATION_ERROR_UNKNOWN, "Error message"));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, CloseConnection) {
  service_ptr_->CloseConnection(presentation_url1_, kPresentationId);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, CloseConnection(_, _, Eq(kPresentationId)))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}

TEST_F(PresentationServiceImplTest, Terminate) {
  service_ptr_->Terminate(presentation_url1_, kPresentationId);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, Terminate(_, _, Eq(kPresentationId)))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}

TEST_F(PresentationServiceImplTest, SetPresentationConnection) {
  PresentationInfo presentation_info(presentation_url1_, kPresentationId);

  blink::mojom::PresentationConnectionPtr connection;
  MockPresentationConnection mock_presentation_connection;
  mojo::Binding<blink::mojom::PresentationConnection> connection_binding(
      &mock_presentation_connection, mojo::MakeRequest(&connection));
  blink::mojom::PresentationConnectionPtr receiver_connection;
  auto request = mojo::MakeRequest(&receiver_connection);

  PresentationInfo expected(presentation_url1_, kPresentationId);
  EXPECT_CALL(mock_delegate_, RegisterOffscreenPresentationConnectionRaw(
                                  _, _, InfoEquals(expected), _));

  service_impl_->SetPresentationConnection(
      presentation_info, std::move(connection), std::move(request));
}

TEST_F(PresentationServiceImplTest, ReceiverPresentationServiceDelegate) {
  MockReceiverPresentationServiceDelegate mock_receiver_delegate;

  PresentationServiceImpl service_impl(contents()->GetMainFrame(), contents(),
                                       nullptr, &mock_receiver_delegate);

  ReceiverConnectionAvailableCallback callback;
  EXPECT_CALL(mock_receiver_delegate,
              RegisterReceiverConnectionAvailableCallback(_))
      .WillOnce(SaveArg<0>(&callback));

  blink::mojom::PresentationServiceClientPtr client_ptr;
  client_binding_.reset(
      new mojo::Binding<blink::mojom::PresentationServiceClient>(
          &mock_client_, mojo::MakeRequest(&client_ptr)));
  service_impl.controller_delegate_ = nullptr;
  service_impl.SetClient(std::move(client_ptr));
  EXPECT_FALSE(callback.is_null());

  // NO-OP for ControllerPresentationServiceDelegate API functions
  EXPECT_CALL(mock_delegate_, ListenForConnectionMessages(_, _, _, _)).Times(0);

  PresentationInfo presentation_info(presentation_url1_, kPresentationId);
  service_impl.ListenForConnectionMessages(presentation_info);
}

TEST_F(PresentationServiceImplTest, StartPresentationInProgress) {
  EXPECT_CALL(mock_delegate_, StartPresentation(_, _, presentation_urls_, _, _))
      .Times(1);
  service_ptr_->StartPresentation(presentation_urls_, base::Bind(&DoNothing));

  // This request should fail immediately, since there is already a
  // StartPresentation in progress.
  service_ptr_->StartPresentation(
      presentation_urls_,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewPresentationCallbackError,
          base::Unretained(this)));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, MaxPendingReconnectPresentationRequests) {
  const char* presentation_url = "http://fooUrl%d";
  const char* presentation_id = "presentationId%d";
  int num_requests = PresentationServiceImpl::kMaxQueuedRequests;
  int i = 0;
  EXPECT_CALL(mock_delegate_, ReconnectPresentation(_, _, _, _, _, _))
      .Times(num_requests);
  for (; i < num_requests; ++i) {
    std::vector<GURL> urls = {GURL(base::StringPrintf(presentation_url, i))};
    service_ptr_->ReconnectPresentation(
        urls, base::StringPrintf(presentation_id, i), base::Bind(&DoNothing));
  }

  std::vector<GURL> urls = {GURL(base::StringPrintf(presentation_url, i))};
  // Exceeded maximum queue size, should invoke mojo callback with error.
  service_ptr_->ReconnectPresentation(
      urls, base::StringPrintf(presentation_id, i),
      base::Bind(
          &PresentationServiceImplTest::ExpectNewPresentationCallbackError,
          base::Unretained(this)));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, ScreenAvailabilityNotSupported) {
  mock_delegate_.set_screen_availability_listening_supported(false);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_client_,
              OnScreenAvailabilityNotSupported(presentation_url1_))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  ListenForScreenAvailabilityAndWait(presentation_url1_, false);
  run_loop.Run();
}

}  // namespace content
