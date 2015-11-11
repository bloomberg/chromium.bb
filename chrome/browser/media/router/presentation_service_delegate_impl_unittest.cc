// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/mock_screen_availability_listener.h"
#include "chrome/browser/media/router/presentation_service_delegate_impl.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/presentation_session.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

namespace media_router {

class MockDelegateObserver
    : public content::PresentationServiceDelegate::Observer {
 public:
  MOCK_METHOD0(OnDelegateDestroyed, void());
  MOCK_METHOD1(OnDefaultPresentationStarted,
               void(const content::PresentationSessionInfo&));
};

class MockDefaultMediaSourceObserver
    : public PresentationServiceDelegateImpl::DefaultMediaSourceObserver {
 public:
  MOCK_METHOD2(OnDefaultMediaSourceChanged,
               void(const MediaSource&, const GURL&));
};

class PresentationServiceDelegateImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::WebContents* wc = web_contents();
    ASSERT_TRUE(wc);
    PresentationServiceDelegateImpl::CreateForWebContents(wc);
    delegate_impl_ = PresentationServiceDelegateImpl::FromWebContents(wc);
    delegate_impl_->SetMediaRouterForTest(&router_);
  }

  PresentationServiceDelegateImpl* delegate_impl_;
  MockMediaRouter router_;
};

TEST_F(PresentationServiceDelegateImplTest, AddScreenAvailabilityListener) {
  ON_CALL(router_, RegisterMediaSinksObserver(_)).WillByDefault(Return(true));

  std::string presentation_url1("http://url1");
  std::string presentation_url2("http://url2");
  MediaSource source1 = MediaSourceForPresentationUrl(presentation_url1);
  MediaSource source2 = MediaSourceForPresentationUrl(presentation_url2);
  MockScreenAvailabilityListener listener1(presentation_url1);
  MockScreenAvailabilityListener listener2(presentation_url2);
  int render_process_id = 10;
  int render_frame_id1 = 1;
  int render_frame_id2 = 2;

  EXPECT_CALL(router_, RegisterMediaSinksObserver(_)).Times(2);
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

TEST_F(PresentationServiceDelegateImplTest, AddSameListenerTwice) {
  ON_CALL(router_, RegisterMediaSinksObserver(_)).WillByDefault(Return(true));

  std::string presentation_url1("http://url1");
  MediaSource source1(MediaSourceForPresentationUrl(presentation_url1));
  MockScreenAvailabilityListener listener1(presentation_url1);
  int render_process_id = 1;
  int render_frame_id = 0;

  EXPECT_CALL(router_, RegisterMediaSinksObserver(_)).Times(1);
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

TEST_F(PresentationServiceDelegateImplTest, SetDefaultPresentationUrl) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("http://www.google.com"));
  content::RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  ASSERT_TRUE(main_frame);
  int render_process_id = main_frame->GetProcess()->GetID();
  int routing_id = main_frame->GetRoutingID();

  std::string presentation_url1("http://foo");
  delegate_impl_->SetDefaultPresentationUrl(render_process_id, routing_id,
                                            presentation_url1);
  EXPECT_TRUE(delegate_impl_->default_source().Equals(
      MediaSourceForPresentationUrl(presentation_url1)));

  // Remove default presentation URL.
  delegate_impl_->SetDefaultPresentationUrl(render_process_id, routing_id, "");
  EXPECT_TRUE(delegate_impl_->default_source().Empty());

  // Set to a new default presentation URL
  std::string presentation_url2("https://youtube.com");
  delegate_impl_->SetDefaultPresentationUrl(render_process_id, routing_id,
                                            presentation_url2);
  EXPECT_TRUE(delegate_impl_->default_source().Equals(
      MediaSourceForPresentationUrl(presentation_url2)));
}

TEST_F(PresentationServiceDelegateImplTest, DefaultMediaSourceObserver) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("http://www.google.com"));
  content::RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  ASSERT_TRUE(main_frame);
  int render_process_id = main_frame->GetProcess()->GetID();
  int routing_id = main_frame->GetRoutingID();
  StrictMock<MockDefaultMediaSourceObserver> observer1;
  StrictMock<MockDefaultMediaSourceObserver> observer2;
  delegate_impl_->AddDefaultMediaSourceObserver(&observer1);
  delegate_impl_->AddDefaultMediaSourceObserver(&observer2);
  std::string url1("http://foo");
  EXPECT_CALL(observer1, OnDefaultMediaSourceChanged(
                             Equals(MediaSourceForPresentationUrl(url1)),
                             GURL("http://www.google.com"))).Times(1);
  EXPECT_CALL(observer2, OnDefaultMediaSourceChanged(
                             Equals(MediaSourceForPresentationUrl(url1)),
                             GURL("http://www.google.com"))).Times(1);
  delegate_impl_->SetDefaultPresentationUrl(render_process_id,
                                            routing_id, url1);

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer1));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer2));

  delegate_impl_->RemoveDefaultMediaSourceObserver(&observer2);
  std::string url2("http://youtube.com");
  EXPECT_CALL(observer1, OnDefaultMediaSourceChanged(
                             Equals(MediaSourceForPresentationUrl(url2)),
                             GURL("http://www.google.com"))).Times(1);
  delegate_impl_->SetDefaultPresentationUrl(render_process_id,
                                            routing_id, url2);

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer1));
  // Remove default presentation URL.
  EXPECT_CALL(observer1, OnDefaultMediaSourceChanged(
                             Equals(MediaSource()),
                             GURL("http://www.google.com"))).Times(1);
  delegate_impl_->SetDefaultPresentationUrl(render_process_id, routing_id, "");
}

TEST_F(PresentationServiceDelegateImplTest, Reset) {
  ON_CALL(router_, RegisterMediaSinksObserver(_)).WillByDefault(Return(true));

  std::string presentation_url1("http://url1");
  MediaSource source = MediaSourceForPresentationUrl(presentation_url1);
  MockScreenAvailabilityListener listener1(presentation_url1);
  int render_process_id = 1;
  int render_frame_id = 0;

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
  scoped_ptr<PresentationServiceDelegateImpl> manager(
      new PresentationServiceDelegateImpl(web_contents()));
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
  ON_CALL(router_, RegisterMediaSinksObserver(_)).WillByDefault(Return(false));

  const std::string presentation_url("http://url1");
  MockScreenAvailabilityListener listener(presentation_url);
  const int render_process_id = 10;
  const int render_frame_id = 1;

  EXPECT_CALL(router_, RegisterMediaSinksObserver(_)).Times(1);
  EXPECT_FALSE(delegate_impl_->AddScreenAvailabilityListener(
      render_process_id, render_frame_id, &listener));
}

}  // namespace media_router
