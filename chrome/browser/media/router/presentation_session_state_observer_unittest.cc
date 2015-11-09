// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/presentation_session_state_observer.h"
#include "content/public/browser/presentation_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media_router {

namespace {

const char kPresentationUrl[] = "http://foo";
const char kPresentationId[] = "presentationId";

MediaRoute::Id CreateRouteId(const char* presentation_url,
                             const char* presentation_id) {
  return base::StringPrintf("urn:x-org.chromium:media:route:%s/cast-sink1/%s",
                            presentation_id, presentation_url);
}

MATCHER_P(PresentationSessionInfoEquals, expected, "") {
  return arg.presentation_url == expected.presentation_url &&
         arg.presentation_id == expected.presentation_id;
}

}  // namespace

class PresentationSessionStateObserverTest : public testing::Test {
 public:
  PresentationSessionStateObserverTest() {}
  ~PresentationSessionStateObserverTest() override {}

 protected:
  void SetUp() override {
    EXPECT_CALL(router_, RegisterMediaRoutesObserver(_));
    observer_.reset(new PresentationSessionStateObserver(
        base::Bind(&PresentationSessionStateObserverTest::OnSessionStateChanged,
                   base::Unretained(this)),
        &route_id_to_presentation_, &router_));
    MediaRoute::Id route_id(CreateRouteId(kPresentationUrl, kPresentationId));
    content::PresentationSessionInfo session_info(kPresentationUrl,
                                                  kPresentationId);
    route_id_to_presentation_.Add(route_id, session_info);
  }

  void TearDown() override {
    if (observer_) {
      EXPECT_CALL(router_, UnregisterMediaRoutesObserver(_));
      observer_.reset();
    }
  }

  MOCK_METHOD2(OnSessionStateChanged,
               void(const content::PresentationSessionInfo& session_info,
                    content::PresentationConnectionState new_state));

  MockMediaRouter router_;
  MediaRouteIdToPresentationSessionMapping route_id_to_presentation_;
  scoped_ptr<PresentationSessionStateObserver> observer_;
};

TEST_F(PresentationSessionStateObserverTest, InvokeCallbackWithConnected) {
  EXPECT_CALL(
      *this, OnSessionStateChanged(
                 PresentationSessionInfoEquals(content::PresentationSessionInfo(
                     kPresentationUrl, kPresentationId)),
                 content::PRESENTATION_CONNECTION_STATE_CONNECTED));
  MediaRoute::Id route_id(CreateRouteId(kPresentationUrl, kPresentationId));
  observer_->OnPresentationSessionConnected(route_id);
}

TEST_F(PresentationSessionStateObserverTest, InvokeCallbackWithDisconnected) {
  content::PresentationSessionInfo session_info(kPresentationUrl,
                                                kPresentationId);
  EXPECT_CALL(*this, OnSessionStateChanged(
                         PresentationSessionInfoEquals(session_info),
                         content::PRESENTATION_CONNECTION_STATE_CONNECTED));
  MediaRoute::Id route_id(CreateRouteId(kPresentationUrl, kPresentationId));
  observer_->OnPresentationSessionConnected(route_id);

  // Route list update is expected to follow creation of route.
  std::vector<MediaRoute> routes;
  routes.push_back(MediaRoute(route_id,
                              MediaSourceForPresentationUrl(kPresentationUrl),
                              "sinkId", "Description", true, "", false));
  observer_->OnRoutesUpdated(routes);

  // New route list does not contain |route_id|, which means it is disconnected.
  EXPECT_CALL(*this, OnSessionStateChanged(
                         PresentationSessionInfoEquals(session_info),
                         content::PRESENTATION_CONNECTION_STATE_CLOSED));
  observer_->OnRoutesUpdated(std::vector<MediaRoute>());

  // Note that it is normally not possible for |route_id| to reappear. But in
  // case it does, test that it does NOT invoke the callback with CONNECTED.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(this));
  observer_->OnRoutesUpdated(routes);
}

TEST_F(PresentationSessionStateObserverTest, Reset) {
  content::PresentationSessionInfo session_info(kPresentationUrl,
                                                kPresentationId);
  EXPECT_CALL(*this, OnSessionStateChanged(
                         PresentationSessionInfoEquals(session_info),
                         content::PRESENTATION_CONNECTION_STATE_CONNECTED));
  MediaRoute::Id route_id(CreateRouteId(kPresentationUrl, kPresentationId));
  observer_->OnPresentationSessionConnected(route_id);

  // Route list update is expected to follow creation of route.
  std::vector<MediaRoute> routes;
  routes.push_back(MediaRoute(route_id,
                              MediaSourceForPresentationUrl(kPresentationUrl),
                              "sinkId", "Description", true, "", false));
  observer_->OnRoutesUpdated(routes);

  // |route_id| is no longer being tracked.
  observer_->Reset();
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(this));
  observer_->OnRoutesUpdated(std::vector<MediaRoute>());
}

}  // namespace media_router
