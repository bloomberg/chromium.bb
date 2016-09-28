// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_service_delegate_impl.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/mock_screen_availability_listener.h"
#include "chrome/browser/media/router/route_request_result.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/presentation_session.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace {

const char kPresentationUrl1[] = "http://foo.fakeUrl.com/";
const char kPresentationUrl2[] = "http://bar.fakeUrl.com/";
const char kFrameUrl[] = "http://anotherFrameUrl.fakeUrl.com/";

}  // namespace

namespace media_router {

class MockDelegateObserver
    : public content::PresentationServiceDelegate::Observer {
 public:
  MOCK_METHOD0(OnDelegateDestroyed, void());
  MOCK_METHOD1(OnDefaultPresentationStarted,
               void(const content::PresentationSessionInfo&));
};

class MockDefaultPresentationRequestObserver
    : public PresentationServiceDelegateImpl::
          DefaultPresentationRequestObserver {
 public:
  MOCK_METHOD1(OnDefaultPresentationChanged, void(const PresentationRequest&));
  MOCK_METHOD0(OnDefaultPresentationRemoved, void());
};

class MockCreatePresentationConnnectionCallbacks {
 public:
  MOCK_METHOD1(OnCreateConnectionSuccess,
               void(const content::PresentationSessionInfo& connection));
  MOCK_METHOD1(OnCreateConnectionError,
               void(const content::PresentationError& error));
};

class PresentationServiceDelegateImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PresentationServiceDelegateImplTest() : delegate_impl_(nullptr) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::WebContents* wc = GetWebContents();
    ASSERT_TRUE(wc);
    PresentationServiceDelegateImpl::CreateForWebContents(wc);
    delegate_impl_ = PresentationServiceDelegateImpl::FromWebContents(wc);
    delegate_impl_->SetMediaRouterForTest(&router_);
  }

  MOCK_METHOD1(OnDefaultPresentationStarted,
               void(const content::PresentationSessionInfo& session_info));

 protected:
  virtual content::WebContents* GetWebContents() { return web_contents(); }

  void RunDefaultPresentationUrlCallbackTest(bool incognito) {
    content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
    ASSERT_TRUE(main_frame);
    int render_process_id = main_frame->GetProcess()->GetID();
    int routing_id = main_frame->GetRoutingID();

    auto callback = base::Bind(
        &PresentationServiceDelegateImplTest::OnDefaultPresentationStarted,
        base::Unretained(this));
    std::vector<std::string> urls({kPresentationUrl1});
    delegate_impl_->SetDefaultPresentationUrls(render_process_id, routing_id,
                                               urls, callback);

    ASSERT_TRUE(delegate_impl_->HasDefaultPresentationRequest());
    PresentationRequest request =
        delegate_impl_->GetDefaultPresentationRequest();

    // Should not trigger callback since route response is error.
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Error", RouteRequestResult::UNKNOWN_ERROR);
    delegate_impl_->OnRouteResponse(request, *result);
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(this));

    // Should not trigger callback since request doesn't match.
    PresentationRequest different_request(RenderFrameHostId(100, 200),
                                          {kPresentationUrl2}, GURL(kFrameUrl));
    MediaRoute* media_route = new MediaRoute(
        "differentRouteId", MediaSourceForPresentationUrl(kPresentationUrl2),
        "mediaSinkId", "", true, "", true);
    media_route->set_incognito(incognito);
    result = RouteRequestResult::FromSuccess(base::WrapUnique(media_route),
                                             "differentPresentationId");
    delegate_impl_->OnRouteResponse(different_request, *result);
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(this));

    // Should trigger callback since request matches.
    EXPECT_CALL(*this, OnDefaultPresentationStarted(_)).Times(1);
    MediaRoute* media_route2 = new MediaRoute(
        "routeId", MediaSourceForPresentationUrl(kPresentationUrl1),
        "mediaSinkId", "", true, "", true);
    media_route2->set_incognito(incognito);
    result = RouteRequestResult::FromSuccess(base::WrapUnique(media_route2),
                                             "presentationId");
    delegate_impl_->OnRouteResponse(request, *result);
  }

  PresentationServiceDelegateImpl* delegate_impl_;
  MockMediaRouter router_;
};

class PresentationServiceDelegateImplIncognitoTest
    : public PresentationServiceDelegateImplTest {
 public:
  PresentationServiceDelegateImplIncognitoTest() :
      incognito_web_contents_(nullptr) {}

 protected:
  content::WebContents* GetWebContents() override {
    if (!incognito_web_contents_) {
      Profile* incognito_profile = profile()->GetOffTheRecordProfile();
      incognito_web_contents_ =
          content::WebContentsTester::CreateTestWebContents(incognito_profile,
                                                            nullptr);
    }
    return incognito_web_contents_;
  }

  void TearDown() override {
    // We must delete the incognito WC first, as that triggers observers which
    // require RenderViewHost, etc., that in turn are deleted by
    // RenderViewHostTestHarness::TearDown().
    delete incognito_web_contents_;
    PresentationServiceDelegateImplTest::TearDown();
  }

  content::WebContents* incognito_web_contents_;
};

TEST_F(PresentationServiceDelegateImplTest, AddScreenAvailabilityListener) {
  MediaSource source1 = MediaSourceForPresentationUrl(kPresentationUrl1);
  MediaSource source2 = MediaSourceForPresentationUrl(kPresentationUrl2);
  MockScreenAvailabilityListener listener1(kPresentationUrl1);
  MockScreenAvailabilityListener listener2(kPresentationUrl2);
  content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
  ASSERT_TRUE(main_frame);
  int render_process_id = main_frame->GetProcess()->GetID();
  int render_frame_id1 = main_frame->GetRoutingID();

  // Note that |render_frame_id2| does not correspond to a real frame. As a
  // result, the observer added with have an empty GURL as origin.
  int render_frame_id2 = 2;

  EXPECT_CALL(router_, RegisterMediaSinksObserver(_))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(delegate_impl_->AddScreenAvailabilityListener(
      render_process_id, render_frame_id1, &listener1));
  EXPECT_TRUE(delegate_impl_->AddScreenAvailabilityListener(
      render_process_id, render_frame_id2, &listener2));
  EXPECT_TRUE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id1, source1.id()))
      << "Mapping not found for " << source1.ToString();
  EXPECT_TRUE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id2, source2.id()))
      << "Mapping not found for " << source2.ToString();

  EXPECT_CALL(router_, UnregisterMediaSinksObserver(_)).Times(2);
  delegate_impl_->RemoveScreenAvailabilityListener(
      render_process_id, render_frame_id1, &listener1);
  delegate_impl_->RemoveScreenAvailabilityListener(
      render_process_id, render_frame_id2, &listener2);
  EXPECT_FALSE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id1, source1.id()));
  EXPECT_FALSE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id2, source2.id()));
}

TEST_F(PresentationServiceDelegateImplTest, AddMultipleListenersToFrame) {
  ON_CALL(router_, RegisterMediaSinksObserver(_)).WillByDefault(Return(true));

  MediaSource source1 = MediaSourceForPresentationUrl(kPresentationUrl1);
  MediaSource source2 = MediaSourceForPresentationUrl(kPresentationUrl2);
  MockScreenAvailabilityListener listener1(kPresentationUrl1);
  MockScreenAvailabilityListener listener2(kPresentationUrl2);
  content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
  ASSERT_TRUE(main_frame);
  int render_process_id = main_frame->GetProcess()->GetID();
  int render_frame_id = main_frame->GetRoutingID();

  EXPECT_CALL(router_, RegisterMediaSinksObserver(_)).Times(2);
  EXPECT_TRUE(delegate_impl_->AddScreenAvailabilityListener(
      render_process_id, render_frame_id, &listener1));
  EXPECT_TRUE(delegate_impl_->AddScreenAvailabilityListener(
      render_process_id, render_frame_id, &listener2));
  EXPECT_TRUE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id, source1.id()))
      << "Mapping not found for " << source1.ToString();
  EXPECT_TRUE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id, source2.id()))
      << "Mapping not found for " << source2.ToString();

  EXPECT_CALL(router_, UnregisterMediaSinksObserver(_)).Times(2);
  delegate_impl_->RemoveScreenAvailabilityListener(
      render_process_id, render_frame_id, &listener1);
  delegate_impl_->RemoveScreenAvailabilityListener(
      render_process_id, render_frame_id, &listener2);
  EXPECT_FALSE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id, source1.id()));
  EXPECT_FALSE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id, source2.id()));
}

TEST_F(PresentationServiceDelegateImplTest, AddSameListenerTwice) {
  MediaSource source1(MediaSourceForPresentationUrl(kPresentationUrl1));
  MockScreenAvailabilityListener listener1(kPresentationUrl1);
  content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
  ASSERT_TRUE(main_frame);
  int render_process_id = main_frame->GetProcess()->GetID();
  int render_frame_id = main_frame->GetRoutingID();

  EXPECT_CALL(router_, RegisterMediaSinksObserver(_)).WillOnce(Return(true));
  EXPECT_TRUE(delegate_impl_->AddScreenAvailabilityListener(
      render_process_id, render_frame_id, &listener1));
  EXPECT_FALSE(delegate_impl_->AddScreenAvailabilityListener(
      render_process_id, render_frame_id, &listener1));
  EXPECT_TRUE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id, source1.id()));

  EXPECT_CALL(router_, UnregisterMediaSinksObserver(_)).Times(1);
  delegate_impl_->RemoveScreenAvailabilityListener(render_process_id,
                                                   render_frame_id, &listener1);
  EXPECT_FALSE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id, source1.id()));
}

// TODO(imcheng): Add a test to set default presentation URL in a different
// RenderFrameHost and verify that it is ignored.
TEST_F(PresentationServiceDelegateImplTest, SetDefaultPresentationUrl) {
  EXPECT_FALSE(delegate_impl_->HasDefaultPresentationRequest());

  GURL frame_url(kFrameUrl);
  content::WebContentsTester::For(GetWebContents())
      ->NavigateAndCommit(frame_url);
  content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
  ASSERT_TRUE(main_frame);
  int render_process_id = main_frame->GetProcess()->GetID();
  int routing_id = main_frame->GetRoutingID();

  auto callback = base::Bind(
      &PresentationServiceDelegateImplTest::OnDefaultPresentationStarted,
      base::Unretained(this));
  std::vector<std::string> urls({kPresentationUrl1});
  delegate_impl_->SetDefaultPresentationUrls(render_process_id, routing_id,
                                             urls, callback);
  ASSERT_TRUE(delegate_impl_->HasDefaultPresentationRequest());
  PresentationRequest request1 =
      delegate_impl_->GetDefaultPresentationRequest();
  EXPECT_EQ(kPresentationUrl1, request1.presentation_url());
  EXPECT_EQ(RenderFrameHostId(render_process_id, routing_id),
            request1.render_frame_host_id());
  EXPECT_EQ(frame_url, request1.frame_url());

  // Set to a new default presentation URL
  std::vector<std::string> new_urls({kPresentationUrl2});
  delegate_impl_->SetDefaultPresentationUrls(render_process_id, routing_id,
                                             new_urls, callback);
  ASSERT_TRUE(delegate_impl_->HasDefaultPresentationRequest());
  PresentationRequest request2 =
      delegate_impl_->GetDefaultPresentationRequest();
  EXPECT_EQ(kPresentationUrl2, request2.presentation_url());
  EXPECT_EQ(RenderFrameHostId(render_process_id, routing_id),
            request2.render_frame_host_id());
  EXPECT_EQ(frame_url, request2.frame_url());

  // Remove default presentation URL.
  delegate_impl_->SetDefaultPresentationUrls(
      render_process_id, routing_id, std::vector<std::string>(), callback);
  EXPECT_FALSE(delegate_impl_->HasDefaultPresentationRequest());
}

TEST_F(PresentationServiceDelegateImplTest, DefaultPresentationUrlCallback) {
  RunDefaultPresentationUrlCallbackTest(false);
}

TEST_F(PresentationServiceDelegateImplIncognitoTest,
       DefaultPresentationUrlCallback) {
  RunDefaultPresentationUrlCallbackTest(true);
}

TEST_F(PresentationServiceDelegateImplTest,
       DefaultPresentationRequestObserver) {
  auto callback = base::Bind(
      &PresentationServiceDelegateImplTest::OnDefaultPresentationStarted,
      base::Unretained(this));

  StrictMock<MockDefaultPresentationRequestObserver> observer;
  delegate_impl_->AddDefaultPresentationRequestObserver(&observer);

  GURL frame_url(kFrameUrl);
  content::WebContentsTester::For(GetWebContents())
      ->NavigateAndCommit(frame_url);
  content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
  ASSERT_TRUE(main_frame);
  int render_process_id = main_frame->GetProcess()->GetID();
  int routing_id = main_frame->GetRoutingID();

  PresentationRequest observed_request1(
      RenderFrameHostId(render_process_id, routing_id), {kPresentationUrl1},
      frame_url);
  EXPECT_CALL(observer, OnDefaultPresentationChanged(Equals(observed_request1)))
      .Times(1);
  std::vector<std::string> request1_urls({kPresentationUrl1});
  delegate_impl_->SetDefaultPresentationUrls(render_process_id, routing_id,
                                             request1_urls, callback);

  ASSERT_TRUE(delegate_impl_->HasDefaultPresentationRequest());
  PresentationRequest request1 =
      delegate_impl_->GetDefaultPresentationRequest();
  EXPECT_TRUE(request1.Equals(observed_request1));

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer));

  PresentationRequest observed_request2(
      RenderFrameHostId(render_process_id, routing_id), {kPresentationUrl2},
      frame_url);
  EXPECT_CALL(observer, OnDefaultPresentationChanged(Equals(observed_request2)))
      .Times(1);
  std::vector<std::string> request2_urls({kPresentationUrl2});
  delegate_impl_->SetDefaultPresentationUrls(render_process_id, routing_id,
                                             request2_urls, callback);
  ASSERT_TRUE(delegate_impl_->HasDefaultPresentationRequest());
  PresentationRequest request2 =
      delegate_impl_->GetDefaultPresentationRequest();
  EXPECT_TRUE(request2.Equals(observed_request2));

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer));

  // Remove default presentation URL.
  EXPECT_CALL(observer, OnDefaultPresentationRemoved()).Times(1);
  delegate_impl_->SetDefaultPresentationUrls(
      render_process_id, routing_id, std::vector<std::string>(), callback);
}

TEST_F(PresentationServiceDelegateImplTest, ListenForConnnectionStateChange) {
  GURL frame_url(kFrameUrl);
  content::WebContentsTester::For(GetWebContents())
      ->NavigateAndCommit(frame_url);
  content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
  ASSERT_TRUE(main_frame);
  int render_process_id = main_frame->GetProcess()->GetID();
  int routing_id = main_frame->GetRoutingID();

  // Set up a PresentationConnection so we can listen to it.
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  EXPECT_CALL(router_, JoinRoute(_, _, _, _, _, _, false))
      .WillOnce(SaveArg<4>(&route_response_callbacks));

  const std::string kPresentationId("pid");
  std::vector<std::string> join_urls({kPresentationUrl1});
  MockCreatePresentationConnnectionCallbacks mock_create_connection_callbacks;
  delegate_impl_->JoinSession(
      render_process_id, routing_id, join_urls, kPresentationId,
      base::Bind(&MockCreatePresentationConnnectionCallbacks::
                     OnCreateConnectionSuccess,
                 base::Unretained(&mock_create_connection_callbacks)),
      base::Bind(
          &MockCreatePresentationConnnectionCallbacks::OnCreateConnectionError,
          base::Unretained(&mock_create_connection_callbacks)));

  EXPECT_CALL(mock_create_connection_callbacks, OnCreateConnectionSuccess(_))
      .Times(1);
  std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromSuccess(
      base::MakeUnique<MediaRoute>(
          "routeId", MediaSourceForPresentationUrl(kPresentationUrl1),
          "mediaSinkId", "description", true, "", true),
      kPresentationId);
  for (const auto& route_response_callback : route_response_callbacks)
    route_response_callback.Run(*result);

  MockPresentationConnectionStateChangedCallback mock_callback;
  content::PresentationConnectionStateChangedCallback callback =
      base::Bind(&MockPresentationConnectionStateChangedCallback::Run,
                 base::Unretained(&mock_callback));
  content::PresentationSessionInfo connection(kPresentationUrl1,
                                              kPresentationId);
  EXPECT_CALL(router_, OnAddPresentationConnectionStateChangedCallbackInvoked(
                           Equals(callback)));
  delegate_impl_->ListenForConnectionStateChange(render_process_id, routing_id,
                                                 connection, callback);
}

TEST_F(PresentationServiceDelegateImplTest, Reset) {
  EXPECT_CALL(router_, RegisterMediaSinksObserver(_))
      .WillRepeatedly(Return(true));

  MediaSource source = MediaSourceForPresentationUrl(kPresentationUrl1);
  MockScreenAvailabilityListener listener1(kPresentationUrl1);

  content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
  ASSERT_TRUE(main_frame);
  int render_process_id = main_frame->GetProcess()->GetID();
  int render_frame_id = main_frame->GetRoutingID();

  EXPECT_TRUE(delegate_impl_->AddScreenAvailabilityListener(
      render_process_id, render_frame_id, &listener1));
  EXPECT_TRUE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id, source.id()));
  EXPECT_CALL(router_, UnregisterMediaSinksObserver(_)).Times(1);
  delegate_impl_->Reset(render_process_id, render_frame_id);
  EXPECT_FALSE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      render_process_id, render_frame_id, source.id()));
}

TEST_F(PresentationServiceDelegateImplTest, DelegateObservers) {
  std::unique_ptr<PresentationServiceDelegateImpl> manager(
      new PresentationServiceDelegateImpl(GetWebContents()));
  manager->SetMediaRouterForTest(&router_);

  StrictMock<MockDelegateObserver> delegate_observer1;
  StrictMock<MockDelegateObserver> delegate_observer2;

  manager->AddObserver(123, 234, &delegate_observer1);
  manager->AddObserver(345, 456, &delegate_observer2);

  // Removes |delegate_observer2|.
  manager->RemoveObserver(345, 456);

  EXPECT_CALL(delegate_observer1, OnDelegateDestroyed()).Times(1);
  manager.reset();
}

TEST_F(PresentationServiceDelegateImplTest, SinksObserverCantRegister) {
  MockScreenAvailabilityListener listener(kPresentationUrl1);
  content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
  ASSERT_TRUE(main_frame);
  int render_process_id = main_frame->GetProcess()->GetID();
  int render_frame_id = main_frame->GetRoutingID();

  EXPECT_CALL(router_, RegisterMediaSinksObserver(_)).WillOnce(Return(false));
  EXPECT_CALL(listener, OnScreenAvailabilityNotSupported());
  EXPECT_FALSE(delegate_impl_->AddScreenAvailabilityListener(
      render_process_id, render_frame_id, &listener));
}

}  // namespace media_router
