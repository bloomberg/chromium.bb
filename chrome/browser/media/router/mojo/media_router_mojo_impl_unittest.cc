// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_test.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/common/media_router/issue.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/version_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/extension.h"
#include "media/base/gmock_callback_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtMost;
using testing::Eq;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::Mock;
using testing::Not;
using testing::Pointee;
using testing::Return;
using testing::ReturnRef;
using testing::Unused;
using testing::SaveArg;
using testing::Sequence;

namespace media_router {

namespace {

const char kDescription[] = "description";
const char kError[] = "error";
const char kSource[] = "source1";
const char kSource2[] = "source2";
const char kRouteId[] = "routeId";
const char kRouteId2[] = "routeId2";
const char kJoinableRouteId[] = "joinableRouteId";
const char kJoinableRouteId2[] = "joinableRouteId2";
const char kSinkId[] = "sink";
const char kSinkId2[] = "sink2";
const char kSinkName[] = "sinkName";
const char kPresentationId[] = "presentationId";
const char kOrigin[] = "http://origin/";
const int kInvalidTabId = -1;
const int kTimeoutMillis = 5 * 1000;

IssueInfo CreateIssueInfo(const std::string& title) {
  IssueInfo issue_info;
  issue_info.title = title;
  issue_info.message = std::string("msg");
  issue_info.default_action = IssueInfo::Action::DISMISS;
  issue_info.severity = IssueInfo::Severity::WARNING;
  return issue_info;
}

// Creates a media route whose ID is |kRouteId|.
MediaRoute CreateMediaRoute() {
  return MediaRoute(kRouteId, MediaSource(kSource), kSinkId, kDescription, true,
                    std::string(), true);
}

// Creates a media route whose ID is |kRouteId2|.
MediaRoute CreateMediaRoute2() {
  return MediaRoute(kRouteId2, MediaSource(kSource), kSinkId, kDescription,
                    true, std::string(), true);
}

void OnCreateMediaRouteController(
    Unused,
    Unused,
    Unused,
    mojom::MediaRouteProvider::CreateMediaRouteControllerCallback& cb) {
  std::move(cb).Run(true);
}

}  // namespace

class RouteResponseCallbackHandler {
 public:
  void Invoke(const RouteRequestResult& result) {
    DoInvoke(result.route(), result.presentation_id(), result.error(),
             result.result_code());
  }
  MOCK_METHOD4(DoInvoke,
               void(const MediaRoute* route,
                    const std::string& presentation_id,
                    const std::string& error_text,
                    RouteRequestResult::ResultCode result_code));
};

class MediaRouterMojoImplTest : public MediaRouterMojoTest {
 public:
  MediaRouterMojoImplTest() {}
  ~MediaRouterMojoImplTest() override {}

 protected:
  void ExpectResultBucketCount(const std::string& operation,
                               RouteRequestResult::ResultCode result_code,
                               int expected_count) {
    histogram_tester_.ExpectBucketCount(
        "MediaRouter.Provider." + operation + ".Result",
        result_code,
        expected_count);
  }

  MediaRouterMojoImpl* SetTestingFactoryAndUse() override {
    return static_cast<MediaRouterMojoImpl*>(
        MediaRouterFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), &CreateMediaRouter));
  }

 private:
  static std::unique_ptr<KeyedService> CreateMediaRouter(
      content::BrowserContext* context) {
    return std::unique_ptr<KeyedService>(new MediaRouterMojoImpl(context));
  }

  base::HistogramTester histogram_tester_;
};

TEST_F(MediaRouterMojoImplTest, CreateRoute) {
  TestCreateRoute();
  ExpectResultBucketCount("CreateRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, CreateIncognitoRoute) {
  MediaSource media_source(kSource);
  MediaRoute expected_route(kRouteId, media_source, kSinkId, "", false, "",
                            false);
  expected_route.set_incognito(true);

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(
      mock_media_route_provider_,
      CreateRouteInternal(kSource, kSinkId, _, url::Origin(GURL(kOrigin)),
                          kInvalidTabId, _, _, _))
      .WillOnce(Invoke([&expected_route](
                           const std::string& source, const std::string& sink,
                           const std::string& presentation_id,
                           const url::Origin& origin, int tab_id,
                           base::TimeDelta timeout, bool incognito,
                           mojom::MediaRouteProvider::CreateRouteCallback& cb) {
        std::move(cb).Run(expected_route, std::string(),
                          RouteRequestResult::OK);
      }));

  base::RunLoop run_loop;
  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, DoInvoke(Pointee(Equals(expected_route)), Not(""), "",
                                RouteRequestResult::OK))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::BindOnce(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->CreateRoute(kSource, kSinkId, url::Origin(GURL(kOrigin)), nullptr,
                        std::move(route_response_callbacks),
                        base::TimeDelta::FromMilliseconds(kTimeoutMillis),
                        true);
  run_loop.Run();
  ExpectResultBucketCount("CreateRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, CreateRouteFails) {
  EXPECT_CALL(
      mock_media_route_provider_,
      CreateRouteInternal(
          kSource, kSinkId, _, url::Origin(GURL(kOrigin)), kInvalidTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), _, _))
      .WillOnce(Invoke([](const std::string& source, const std::string& sink,
                          const std::string& presentation_id,
                          const url::Origin& origin, int tab_id,
                          base::TimeDelta timeout, bool incognito,
                          mojom::MediaRouteProvider::CreateRouteCallback& cb) {
        std::move(cb).Run(base::nullopt, std::string(kError),
                          RouteRequestResult::TIMED_OUT);
      }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", kError, RouteRequestResult::TIMED_OUT))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::BindOnce(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->CreateRoute(kSource, kSinkId, url::Origin(GURL(kOrigin)), nullptr,
                        std::move(route_response_callbacks),
                        base::TimeDelta::FromMilliseconds(kTimeoutMillis),
                        false);
  run_loop.Run();
  ExpectResultBucketCount("CreateRoute", RouteRequestResult::TIMED_OUT, 1);
}

TEST_F(MediaRouterMojoImplTest, CreateRouteIncognitoMismatchFails) {
  EXPECT_CALL(
      mock_media_route_provider_,
      CreateRouteInternal(
          kSource, kSinkId, _, url::Origin(GURL(kOrigin)), kInvalidTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(Invoke([](const std::string& source, const std::string& sink,
                          const std::string& presentation_id,
                          const url::Origin& origin, int tab_id,
                          base::TimeDelta timeout, bool incognito,
                          mojom::MediaRouteProvider::CreateRouteCallback& cb) {
        std::move(cb).Run(CreateMediaRoute(), std::string(),
                          RouteRequestResult::OK);
      }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  std::string error("Mismatch in incognito status: request = 1, response = 0");
  EXPECT_CALL(handler, DoInvoke(nullptr, "", error,
                                RouteRequestResult::INCOGNITO_MISMATCH))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::BindOnce(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->CreateRoute(kSource, kSinkId, url::Origin(GURL(kOrigin)), nullptr,
                        std::move(route_response_callbacks),
                        base::TimeDelta::FromMilliseconds(kTimeoutMillis),
                        true);
  run_loop.Run();
  ExpectResultBucketCount("CreateRoute", RouteRequestResult::INCOGNITO_MISMATCH,
                          1);
}

TEST_F(MediaRouterMojoImplTest, IncognitoRoutesTerminatedOnProfileShutdown) {
  MediaRoute route = CreateMediaRoute();
  route.set_incognito(true);

  EXPECT_CALL(
      mock_media_route_provider_,
      CreateRouteInternal(
          kSource, kSinkId, _, url::Origin(GURL(kOrigin)), kInvalidTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(
          Invoke([&route](const std::string& source, const std::string& sink,
                          const std::string& presentation_id,
                          const url::Origin& origin, int tab_id,
                          base::TimeDelta timeout, bool incognito,
                          mojom::MediaRouteProvider::CreateRouteCallback& cb) {
            std::move(cb).Run(route, std::string(), RouteRequestResult::OK);
          }));
  base::RunLoop run_loop;
  router()->CreateRoute(kSource, kSinkId, url::Origin(GURL(kOrigin)), nullptr,
                        std::vector<MediaRouteResponseCallback>(),
                        base::TimeDelta::FromMilliseconds(kTimeoutMillis),
                        true);
  std::vector<MediaRoute> routes;
  routes.push_back(route);
  router()->OnRoutesUpdated(routes, std::string(), std::vector<std::string>());

  // TODO(mfoltz): Where possible, convert other tests to use RunUntilIdle
  // instead of manually calling Run/Quit on the run loop.
  run_loop.RunUntilIdle();

  EXPECT_CALL(mock_media_route_provider_, TerminateRouteInternal(kRouteId, _))
      .WillOnce(
          Invoke([](const std::string& route_id,
                    mojom::MediaRouteProvider::TerminateRouteCallback& cb) {
            std::move(cb).Run(base::nullopt, RouteRequestResult::OK);
          }));

  base::RunLoop run_loop2;
  router()->OnIncognitoProfileShutdown();
  run_loop2.RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, JoinRoute) {
  TestJoinRoute();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, JoinRouteNotFoundFails) {
  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler, DoInvoke(nullptr, "", "Route not found",
                                RouteRequestResult::ROUTE_NOT_FOUND))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::BindOnce(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->JoinRoute(kSource, kPresentationId, url::Origin(GURL(kOrigin)),
                      nullptr, std::move(route_response_callbacks),
                      base::TimeDelta::FromMilliseconds(kTimeoutMillis), false);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::ROUTE_NOT_FOUND, 1);
}

TEST_F(MediaRouterMojoImplTest, JoinRouteTimedOutFails) {
  // Make sure the MR has received an update with the route, so it knows there
  // is a route to join.
  std::vector<MediaRoute> routes;
  routes.push_back(CreateMediaRoute());
  router()->OnRoutesUpdated(routes, std::string(), std::vector<std::string>());
  EXPECT_TRUE(router()->HasJoinableRoute());

  EXPECT_CALL(
      mock_media_route_provider_,
      JoinRouteInternal(
          kSource, kPresentationId, url::Origin(GURL(kOrigin)), kInvalidTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), _, _))
      .WillOnce(Invoke(
          [](const std::string& source, const std::string& presentation_id,
             const url::Origin& origin, int tab_id, base::TimeDelta timeout,
             bool incognito, mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            std::move(cb).Run(base::nullopt, std::string(kError),
                              RouteRequestResult::TIMED_OUT);
          }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", kError, RouteRequestResult::TIMED_OUT))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::BindOnce(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->JoinRoute(kSource, kPresentationId, url::Origin(GURL(kOrigin)),
                      nullptr, std::move(route_response_callbacks),
                      base::TimeDelta::FromMilliseconds(kTimeoutMillis), false);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::TIMED_OUT, 1);
}

TEST_F(MediaRouterMojoImplTest, JoinRouteIncognitoMismatchFails) {
  MediaRoute route = CreateMediaRoute();

  // Make sure the MR has received an update with the route, so it knows there
  // is a route to join.
  std::vector<MediaRoute> routes;
  routes.push_back(route);
  router()->OnRoutesUpdated(routes, std::string(), std::vector<std::string>());
  EXPECT_TRUE(router()->HasJoinableRoute());

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(
      mock_media_route_provider_,
      JoinRouteInternal(
          kSource, kPresentationId, url::Origin(GURL(kOrigin)), kInvalidTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(
          Invoke([&route](const std::string& source,
                          const std::string& presentation_id,
                          const url::Origin& origin, int tab_id,
                          base::TimeDelta timeout, bool incognito,
                          mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            std::move(cb).Run(route, std::string(), RouteRequestResult::OK);
          }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  std::string error("Mismatch in incognito status: request = 1, response = 0");
  EXPECT_CALL(handler, DoInvoke(nullptr, "", error,
                                RouteRequestResult::INCOGNITO_MISMATCH))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::BindOnce(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->JoinRoute(kSource, kPresentationId, url::Origin(GURL(kOrigin)),
                      nullptr, std::move(route_response_callbacks),
                      base::TimeDelta::FromMilliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::INCOGNITO_MISMATCH,
                          1);
}

TEST_F(MediaRouterMojoImplTest, ConnectRouteByRouteId) {
  TestConnectRouteByRouteId();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, ConnectRouteByRouteIdFails) {
  EXPECT_CALL(
      mock_media_route_provider_,
      ConnectRouteByRouteIdInternal(
          kSource, kRouteId, _, url::Origin(GURL(kOrigin)), kInvalidTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(Invoke(
          [](const std::string& source, const std::string& route_id,
             const std::string& presentation_id, const url::Origin& origin,
             int tab_id, base::TimeDelta timeout, bool incognito,
             mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            std::move(cb).Run(base::nullopt, std::string(kError),
                              RouteRequestResult::TIMED_OUT);
          }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", kError, RouteRequestResult::TIMED_OUT))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::BindOnce(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->ConnectRouteByRouteId(
      kSource, kRouteId, url::Origin(GURL(kOrigin)), nullptr,
      std::move(route_response_callbacks),
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::TIMED_OUT, 1);
}

TEST_F(MediaRouterMojoImplTest, ConnectRouteByIdIncognitoMismatchFails) {
  MediaRoute route = CreateMediaRoute();

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(
      mock_media_route_provider_,
      ConnectRouteByRouteIdInternal(
          kSource, kRouteId, _, url::Origin(GURL(kOrigin)), kInvalidTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(Invoke(
          [&route](const std::string& source, const std::string& route_id,
                   const std::string& presentation_id,
                   const url::Origin& origin, int tab_id,
                   base::TimeDelta timeout, bool incognito,
                   mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            std::move(cb).Run(route, std::string(), RouteRequestResult::OK);
          }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  std::string error("Mismatch in incognito status: request = 1, response = 0");
  EXPECT_CALL(handler, DoInvoke(nullptr, "", error,
                                RouteRequestResult::INCOGNITO_MISMATCH))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::BindOnce(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->ConnectRouteByRouteId(
      kSource, kRouteId, url::Origin(GURL(kOrigin)), nullptr,
      std::move(route_response_callbacks),
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::INCOGNITO_MISMATCH,
                          1);
}

TEST_F(MediaRouterMojoImplTest, DetachRoute) {
  TestDetachRoute();
}

TEST_F(MediaRouterMojoImplTest, TerminateRoute) {
  TestTerminateRoute();
  ExpectResultBucketCount("TerminateRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, TerminateRouteFails) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_media_route_provider_, TerminateRouteInternal(kRouteId, _))
      .WillOnce(
          Invoke([](const std::string& route_id,
                    mojom::MediaRouteProvider::TerminateRouteCallback& cb) {
            std::move(cb).Run(std::string("timed out"),
                              RouteRequestResult::TIMED_OUT);
          }));
  router()->TerminateRoute(kRouteId);
  run_loop.RunUntilIdle();
  ExpectResultBucketCount("TerminateRoute", RouteRequestResult::OK, 0);
  ExpectResultBucketCount("TerminateRoute", RouteRequestResult::TIMED_OUT, 1);
}

TEST_F(MediaRouterMojoImplTest, HandleIssue) {
  MockIssuesObserver issue_observer1(router());
  MockIssuesObserver issue_observer2(router());
  issue_observer1.Init();
  issue_observer2.Init();

  IssueInfo issue_info = CreateIssueInfo("title 1");

  Issue issue_from_observer1((IssueInfo()));
  Issue issue_from_observer2((IssueInfo()));
  EXPECT_CALL(issue_observer1, OnIssue(_))
      .WillOnce(SaveArg<0>(&issue_from_observer1));
  EXPECT_CALL(issue_observer2, OnIssue(_))
      .WillOnce(SaveArg<0>(&issue_from_observer2));

  router()->OnIssue(issue_info);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(issue_from_observer1.id(), issue_from_observer2.id());
  EXPECT_EQ(issue_info, issue_from_observer1.info());
  EXPECT_EQ(issue_info, issue_from_observer2.info());

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&issue_observer1));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&issue_observer2));

  EXPECT_CALL(issue_observer1, OnIssuesCleared());
  EXPECT_CALL(issue_observer2, OnIssuesCleared());

  router()->ClearIssue(issue_from_observer1.id());

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&issue_observer1));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&issue_observer2));
}

TEST_F(MediaRouterMojoImplTest, RegisterAndUnregisterMediaSinksObserver) {
  router()->OnSinkAvailabilityUpdated(
      mojom::MediaRouter::SinkAvailability::AVAILABLE);
  MediaSource media_source(kSource);

  // These should only be called once even if there is more than one observer
  // for a given source.
  EXPECT_CALL(mock_media_route_provider_, StartObservingMediaSinks(kSource));
  EXPECT_CALL(mock_media_route_provider_, StartObservingMediaSinks(kSource2));

  auto sinks_observer = base::MakeUnique<MockMediaSinksObserver>(
      router(), media_source, url::Origin(GURL(kOrigin)));
  EXPECT_TRUE(sinks_observer->Init());
  auto extra_sinks_observer = base::MakeUnique<MockMediaSinksObserver>(
      router(), media_source, url::Origin(GURL(kOrigin)));
  EXPECT_TRUE(extra_sinks_observer->Init());
  auto unrelated_sinks_observer = base::MakeUnique<MockMediaSinksObserver>(
      router(), MediaSource(kSource2), url::Origin(GURL(kOrigin)));
  EXPECT_TRUE(unrelated_sinks_observer->Init());
  ProcessEventLoop();

  std::vector<MediaSink> expected_sinks;
  expected_sinks.push_back(MediaSink(kSinkId, kSinkName, SinkIconType::CAST));
  expected_sinks.push_back(MediaSink(kSinkId2, kSinkName, SinkIconType::CAST));

  std::vector<MediaSinkInternal> sinks;
  for (const auto& expected_sink : expected_sinks) {
    MediaSinkInternal sink_internal;
    sink_internal.set_sink(expected_sink);
    sinks.push_back(sink_internal);
  }

  base::RunLoop run_loop;
  EXPECT_CALL(*sinks_observer, OnSinksReceived(SequenceEquals(expected_sinks)));
  EXPECT_CALL(*extra_sinks_observer,
              OnSinksReceived(SequenceEquals(expected_sinks)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->OnSinksReceived(
      media_source.id(), sinks,
      std::vector<url::Origin>(1, url::Origin(GURL(kOrigin))));
  run_loop.Run();

  // Since the MediaRouterMojoImpl has already received results for
  // |media_source|, return cached results to observers that are subsequently
  // registered.
  auto cached_sinks_observer = base::MakeUnique<MockMediaSinksObserver>(
      router(), media_source, url::Origin(GURL(kOrigin)));
  EXPECT_CALL(*cached_sinks_observer,
              OnSinksReceived(SequenceEquals(expected_sinks)));
  EXPECT_TRUE(cached_sinks_observer->Init());

  // Different origin from cached result. Empty list will be returned.
  auto cached_sinks_observer2 = base::MakeUnique<MockMediaSinksObserver>(
      router(), media_source, url::Origin(GURL("https://youtube.com")));
  EXPECT_CALL(*cached_sinks_observer2, OnSinksReceived(IsEmpty()));
  EXPECT_TRUE(cached_sinks_observer2->Init());

  base::RunLoop run_loop2;
  EXPECT_CALL(mock_media_route_provider_, StopObservingMediaSinks(kSource));
  EXPECT_CALL(mock_media_route_provider_, StopObservingMediaSinks(kSource2))
      .WillOnce(InvokeWithoutArgs([&run_loop2]() { run_loop2.Quit(); }));
  sinks_observer.reset();
  extra_sinks_observer.reset();
  unrelated_sinks_observer.reset();
  cached_sinks_observer.reset();
  cached_sinks_observer2.reset();
  run_loop2.Run();
}

TEST_F(MediaRouterMojoImplTest,
       RegisterMediaSinksObserverWithAvailabilityChange) {
  // When availability is UNAVAILABLE, no calls should be made to MRPM.
  router()->OnSinkAvailabilityUpdated(
      mojom::MediaRouter::SinkAvailability::UNAVAILABLE);
  MediaSource media_source(kSource);
  auto sinks_observer = base::MakeUnique<MockMediaSinksObserver>(
      router(), media_source, url::Origin(GURL(kOrigin)));
  EXPECT_CALL(*sinks_observer, OnSinksReceived(IsEmpty()));
  EXPECT_TRUE(sinks_observer->Init());
  MediaSource media_source2(kSource2);
  auto sinks_observer2 = base::MakeUnique<MockMediaSinksObserver>(
      router(), media_source2, url::Origin(GURL(kOrigin)));
  EXPECT_CALL(*sinks_observer2, OnSinksReceived(IsEmpty()));
  EXPECT_TRUE(sinks_observer2->Init());
  EXPECT_CALL(mock_media_route_provider_, StartObservingMediaSinks(kSource))
      .Times(0);
  EXPECT_CALL(mock_media_route_provider_, StartObservingMediaSinks(kSource2))
      .Times(0);
  ProcessEventLoop();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_media_route_provider_));

  // When availability transitions AVAILABLE, existing sink queries should be
  // sent to MRPM.
  router()->OnSinkAvailabilityUpdated(
      mojom::MediaRouter::SinkAvailability::AVAILABLE);
  EXPECT_CALL(mock_media_route_provider_, StartObservingMediaSinks(kSource))
      .Times(1);
  EXPECT_CALL(mock_media_route_provider_, StartObservingMediaSinks(kSource2))
      .Times(1);
  ProcessEventLoop();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_media_route_provider_));

  // No change in availability status; no calls should be made to MRPM.
  router()->OnSinkAvailabilityUpdated(
      mojom::MediaRouter::SinkAvailability::AVAILABLE);
  EXPECT_CALL(mock_media_route_provider_, StartObservingMediaSinks(kSource))
      .Times(0);
  EXPECT_CALL(mock_media_route_provider_, StartObservingMediaSinks(kSource2))
      .Times(0);
  ProcessEventLoop();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_media_route_provider_));

  // When availability is UNAVAILABLE, queries are already removed from MRPM.
  // Unregistering observer won't result in call to MRPM to remove query.
  router()->OnSinkAvailabilityUpdated(
      mojom::MediaRouter::SinkAvailability::UNAVAILABLE);
  EXPECT_CALL(mock_media_route_provider_, StopObservingMediaSinks(kSource))
      .Times(0);
  sinks_observer.reset();
  ProcessEventLoop();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_media_route_provider_));

  // When availability is AVAILABLE, call is made to MRPM to remove query when
  // observer is unregistered.
  router()->OnSinkAvailabilityUpdated(
      mojom::MediaRouter::SinkAvailability::AVAILABLE);
  EXPECT_CALL(mock_media_route_provider_, StopObservingMediaSinks(kSource2));
  sinks_observer2.reset();
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, RegisterAndUnregisterMediaRoutesObserver) {
  MockMediaRouter mock_router;
  MediaSource media_source(kSource);
  MediaSource different_media_source(kSource2);
  EXPECT_CALL(mock_media_route_provider_,
              StartObservingMediaRoutes(media_source.id()))
      .Times(1);
  EXPECT_CALL(
      mock_media_route_provider_,
      StartObservingMediaRoutes(different_media_source.id()))
      .Times(1);

  MediaRoutesObserver* observer_captured;
  EXPECT_CALL(mock_router, RegisterMediaRoutesObserver(_))
      .Times(3)
      .WillRepeatedly(SaveArg<0>(&observer_captured));
  MockMediaRoutesObserver routes_observer(&mock_router, media_source.id());
  EXPECT_EQ(observer_captured, &routes_observer);
  MockMediaRoutesObserver extra_routes_observer(&mock_router,
                                                media_source.id());
  EXPECT_EQ(observer_captured, &extra_routes_observer);
  MockMediaRoutesObserver different_routes_observer(
      &mock_router, different_media_source.id());
  EXPECT_EQ(observer_captured, &different_routes_observer);
  router()->RegisterMediaRoutesObserver(&routes_observer);
  router()->RegisterMediaRoutesObserver(&extra_routes_observer);
  router()->RegisterMediaRoutesObserver(&different_routes_observer);

  std::vector<MediaRoute> expected_routes;
  expected_routes.push_back(MediaRoute(kRouteId, media_source, kSinkId,
                                       kDescription, false, "", false));
  MediaRoute incognito_expected_route(kRouteId2, media_source, kSinkId,
                                      kDescription, false, "", false);
  incognito_expected_route.set_incognito(true);
  expected_routes.push_back(incognito_expected_route);
  std::vector<MediaRoute::Id> expected_joinable_route_ids;
  expected_joinable_route_ids.push_back(kJoinableRouteId);
  expected_joinable_route_ids.push_back(kJoinableRouteId2);

  EXPECT_CALL(routes_observer, OnRoutesUpdated(SequenceEquals(expected_routes),
                                               expected_joinable_route_ids));
  EXPECT_CALL(extra_routes_observer,
              OnRoutesUpdated(SequenceEquals(expected_routes),
                              expected_joinable_route_ids));
  EXPECT_CALL(different_routes_observer,
              OnRoutesUpdated(SequenceEquals(expected_routes),
                              expected_joinable_route_ids))
      .Times(0);
  router()->OnRoutesUpdated(expected_routes, media_source.id(),
                            expected_joinable_route_ids);
  ProcessEventLoop();

  EXPECT_CALL(mock_router, UnregisterMediaRoutesObserver(&routes_observer));
  EXPECT_CALL(mock_router,
              UnregisterMediaRoutesObserver(&extra_routes_observer));
  EXPECT_CALL(mock_router,
              UnregisterMediaRoutesObserver(&different_routes_observer));
  router()->UnregisterMediaRoutesObserver(&routes_observer);
  router()->UnregisterMediaRoutesObserver(&extra_routes_observer);
  router()->UnregisterMediaRoutesObserver(&different_routes_observer);
  EXPECT_CALL(mock_media_route_provider_,
              StopObservingMediaRoutes(media_source.id()))
      .Times(1);
  EXPECT_CALL(
      mock_media_route_provider_,
      StopObservingMediaRoutes(different_media_source.id()));
  ProcessEventLoop();
}

// Tests that multiple MediaRoutesObservers having the same query do not cause
// extra extension wake-ups because the OnRoutesUpdated() results are cached.
TEST_F(MediaRouterMojoImplTest, RegisterMediaRoutesObserver_DedupingWithCache) {
  const MediaSource media_source = MediaSource(kSource);
  std::vector<MediaRoute> expected_routes;
  expected_routes.push_back(MediaRoute(kRouteId, media_source, kSinkId,
                                       kDescription, false, "", false));
  std::vector<MediaRoute::Id> expected_joinable_route_ids;
  expected_joinable_route_ids.push_back(kJoinableRouteId);

  Sequence sequence;

  // Creating the first observer will wake-up the provider and ask it to start
  // observing routes having source |kSource|. The provider will respond with
  // the existing route.
  EXPECT_CALL(mock_media_route_provider_,
              StartObservingMediaRoutes(media_source.id()))
      .Times(1);
  auto observer1 =
      base::MakeUnique<MockMediaRoutesObserver>(router(), media_source.id());
  ProcessEventLoop();
  EXPECT_CALL(*observer1, OnRoutesUpdated(SequenceEquals(expected_routes),
                                          expected_joinable_route_ids))
      .Times(1);
  router()->OnRoutesUpdated(expected_routes, media_source.id(),
                            expected_joinable_route_ids);
  ProcessEventLoop();

  // Creating two more observers will not wake up the provider. Instead, the
  // cached route list will be returned.
  auto observer2 =
      base::MakeUnique<MockMediaRoutesObserver>(router(), media_source.id());
  auto observer3 =
      base::MakeUnique<MockMediaRoutesObserver>(router(), media_source.id());
  EXPECT_CALL(*observer2, OnRoutesUpdated(SequenceEquals(expected_routes),
                                          expected_joinable_route_ids))
      .Times(1);
  EXPECT_CALL(*observer3, OnRoutesUpdated(SequenceEquals(expected_routes),
                                          expected_joinable_route_ids))
      .Times(1);
  ProcessEventLoop();

  // Kill 2 of three observers, and expect nothing happens at the provider.
  observer1.reset();
  observer2.reset();
  ProcessEventLoop();

  // Kill the final observer, and expect the provider to be woken-up and called
  // with the "stop observing" notification.
  EXPECT_CALL(mock_media_route_provider_,
              StopObservingMediaRoutes(media_source.id()))
      .Times(1);
  observer3.reset();
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, SendRouteMessage) {
  TestSendRouteMessage();
}

TEST_F(MediaRouterMojoImplTest, SendRouteBinaryMessage) {
  TestSendRouteBinaryMessage();
}

namespace {

// Used in the RouteMessages* tests to populate the messages that will be
// processed and dispatched to RouteMessageObservers.
void PopulateRouteMessages(
    std::vector<content::PresentationConnectionMessage>* batch1,
    std::vector<content::PresentationConnectionMessage>* batch2,
    std::vector<content::PresentationConnectionMessage>* batch3,
    std::vector<content::PresentationConnectionMessage>* all_messages) {
  batch1->clear();
  batch2->clear();
  batch3->clear();
  batch1->emplace_back("text1");
  batch2->emplace_back(std::vector<uint8_t>(1, UINT8_C(1)));
  batch2->emplace_back("text2");
  batch3->emplace_back("text3");
  batch3->emplace_back(std::vector<uint8_t>(1, UINT8_C(2)));
  batch3->emplace_back(std::vector<uint8_t>(1, UINT8_C(3)));
  all_messages->clear();
  all_messages->insert(all_messages->end(), batch1->begin(), batch1->end());
  all_messages->insert(all_messages->end(), batch2->begin(), batch2->end());
  all_messages->insert(all_messages->end(), batch3->begin(), batch3->end());
}

// Used in the RouteMessages* tests to observe and sanity-check that the
// messages being received from the router are correct and in-sequence. The
// checks here correspond to the expected messages in PopulateRouteMessages()
// above.
class ExpectedMessagesObserver : public RouteMessageObserver {
 public:
  ExpectedMessagesObserver(
      MediaRouter* router,
      const MediaRoute::Id& route_id,
      const std::vector<content::PresentationConnectionMessage>&
          expected_messages)
      : RouteMessageObserver(router, route_id),
        expected_messages_(expected_messages) {}

  ~ExpectedMessagesObserver() final {
    CheckReceivedMessages();
  }

 private:
  void OnMessagesReceived(
      const std::vector<content::PresentationConnectionMessage>& messages)
      final {
    messages_.insert(messages_.end(), messages.begin(), messages.end());
  }

  void CheckReceivedMessages() {
    ASSERT_EQ(expected_messages_.size(), messages_.size());
    for (size_t i = 0; i < expected_messages_.size(); i++) {
      EXPECT_EQ(expected_messages_[i], messages_[i])
          << "Message mismatch at index " << i << ": expected: "
          << PresentationConnectionMessageToString(expected_messages_[i])
          << ", actual: "
          << PresentationConnectionMessageToString(messages_[i]);
    }
  }

  std::vector<content::PresentationConnectionMessage> expected_messages_;
  std::vector<content::PresentationConnectionMessage> messages_;
};

}  // namespace

TEST_F(MediaRouterMojoImplTest, RouteMessagesSingleObserver) {
  std::vector<content::PresentationConnectionMessage> incoming_batch1,
      incoming_batch2, incoming_batch3, all_messages;
  PopulateRouteMessages(&incoming_batch1, &incoming_batch2, &incoming_batch3,
                        &all_messages);

  base::RunLoop run_loop;
  MediaRoute::Id expected_route_id("foo");
  EXPECT_CALL(mock_media_route_provider_,
              StartListeningForRouteMessages(Eq(expected_route_id)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // Creating ExpectedMessagesObserver will register itself to the
  // MediaRouter, which in turn will start listening for route messages.
  ExpectedMessagesObserver observer(router(), expected_route_id, all_messages);
  run_loop.Run();  // Will quit when StartListeningForRouteMessages() is called.
  router()->OnRouteMessagesReceived(expected_route_id, incoming_batch1);
  router()->OnRouteMessagesReceived(expected_route_id, incoming_batch2);
  router()->OnRouteMessagesReceived(expected_route_id, incoming_batch3);
  // When |observer| goes out-of-scope, its destructor will ensure all expected
  // messages have been received.
}

TEST_F(MediaRouterMojoImplTest, RouteMessagesMultipleObservers) {
  std::vector<content::PresentationConnectionMessage> incoming_batch1,
      incoming_batch2, incoming_batch3, all_messages;
  PopulateRouteMessages(&incoming_batch1, &incoming_batch2, &incoming_batch3,
                        &all_messages);

  base::RunLoop run_loop;
  MediaRoute::Id expected_route_id("foo");
  EXPECT_CALL(mock_media_route_provider_,
              StartListeningForRouteMessages(Eq(expected_route_id)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // The ExpectedMessagesObservers will register themselves with the
  // MediaRouter, which in turn will start listening for route messages.
  ExpectedMessagesObserver observer1(router(), expected_route_id, all_messages);
  ExpectedMessagesObserver observer2(router(), expected_route_id, all_messages);
  run_loop.Run();  // Will quit when StartListeningForRouteMessages() is called.
  router()->OnRouteMessagesReceived(expected_route_id, incoming_batch1);
  router()->OnRouteMessagesReceived(expected_route_id, incoming_batch2);
  router()->OnRouteMessagesReceived(expected_route_id, incoming_batch3);
  // As each |observer| goes out-of-scope, its destructor will ensure all
  // expected messages have been received.
}

TEST_F(MediaRouterMojoImplTest, PresentationConnectionStateChangedCallback) {
  MediaRoute::Id route_id("route-id");
  const GURL presentation_url("http://www.example.com/presentation.html");
  const std::string kPresentationId("pid");
  content::PresentationInfo connection(presentation_url, kPresentationId);
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback;
  std::unique_ptr<PresentationConnectionStateSubscription> subscription =
      router()->AddPresentationConnectionStateChangedCallback(route_id,
                                                              callback.Get());

  {
    base::RunLoop run_loop;
    content::PresentationConnectionStateChangeInfo closed_info(
        content::PRESENTATION_CONNECTION_STATE_CLOSED);
    closed_info.close_reason =
        content::PRESENTATION_CONNECTION_CLOSE_REASON_WENT_AWAY;
    closed_info.message = "Foo";

    EXPECT_CALL(callback, Run(StateChangeInfoEquals(closed_info)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    router()->OnPresentationConnectionClosed(
        route_id, content::PRESENTATION_CONNECTION_CLOSE_REASON_WENT_AWAY,
        "Foo");
    run_loop.Run();
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&callback));
  }

  content::PresentationConnectionStateChangeInfo terminated_info(
      content::PRESENTATION_CONNECTION_STATE_TERMINATED);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(callback, Run(StateChangeInfoEquals(terminated_info)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    router()->OnPresentationConnectionStateChanged(
        route_id, content::PRESENTATION_CONNECTION_STATE_TERMINATED);
    run_loop.Run();

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&callback));
  }
}

TEST_F(MediaRouterMojoImplTest,
       PresentationConnectionStateChangedCallbackRemoved) {
  MediaRoute::Id route_id("route-id");
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback;
  std::unique_ptr<PresentationConnectionStateSubscription> subscription =
      router()->AddPresentationConnectionStateChangedCallback(route_id,
                                                              callback.Get());

  // Callback has been removed, so we don't expect it to be called anymore.
  subscription.reset();
  EXPECT_TRUE(router()->presentation_connection_state_callbacks_.empty());

  EXPECT_CALL(callback, Run(_)).Times(0);
  router()->OnPresentationConnectionStateChanged(
      route_id, content::PRESENTATION_CONNECTION_STATE_TERMINATED);
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, SearchSinks) {
  TestSearchSinks();
}

TEST_F(MediaRouterMojoImplTest, ProvideSinks) {
  TestProvideSinks();
}

TEST_F(MediaRouterMojoImplTest, GetRouteController) {
  TestCreateMediaRouteController();
}

TEST_F(MediaRouterMojoImplTest, GetRouteControllerMultipleTimes) {
  router()->OnRoutesUpdated({CreateMediaRoute(), CreateMediaRoute2()},
                            std::string(), std::vector<std::string>());

  EXPECT_CALL(mock_media_route_provider_,
              CreateMediaRouteControllerInternal(kRouteId, _, _, _))
      .WillOnce(Invoke(OnCreateMediaRouteController));
  scoped_refptr<MediaRouteController> route_controller1a =
      router()->GetRouteController(kRouteId);

  // Calling GetRouteController() with the same route ID for the second time
  // (without destroying the MediaRouteController first) should not result in a
  // CreateMediaRouteController() call.
  scoped_refptr<MediaRouteController> route_controller1b =
      router()->GetRouteController(kRouteId);

  // The same MediaRouteController instance should have been returned.
  EXPECT_EQ(route_controller1a.get(), route_controller1b.get());

  // Calling GetRouteController() with another route ID should result in a
  // CreateMediaRouteController() call.
  EXPECT_CALL(mock_media_route_provider_,
              CreateMediaRouteControllerInternal(kRouteId2, _, _, _))
      .WillOnce(Invoke(OnCreateMediaRouteController));
  scoped_refptr<MediaRouteController> route_controller2 =
      router()->GetRouteController(kRouteId2);

  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, GetRouteControllerAfterInvalidation) {
  router()->OnRoutesUpdated({CreateMediaRoute()}, std::string(),
                            std::vector<std::string>());

  EXPECT_CALL(mock_media_route_provider_,
              CreateMediaRouteControllerInternal(kRouteId, _, _, _))
      .Times(2)
      .WillRepeatedly(Invoke(OnCreateMediaRouteController));

  scoped_refptr<MediaRouteController> route_controller =
      router()->GetRouteController(kRouteId);
  // Invalidate the MediaRouteController.
  route_controller = nullptr;
  // Call again with the same route ID. Since we've invalidated the
  // MediaRouteController, CreateMediaRouteController() should be called again.
  route_controller = router()->GetRouteController(kRouteId);

  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, GetRouteControllerAfterRouteInvalidation) {
  router()->OnRoutesUpdated({CreateMediaRoute(), CreateMediaRoute2()},
                            std::string(), std::vector<std::string>());

  EXPECT_CALL(mock_media_route_provider_,
              CreateMediaRouteControllerInternal(kRouteId, _, _, _))
      .WillOnce(Invoke(OnCreateMediaRouteController));
  EXPECT_CALL(mock_media_route_provider_,
              CreateMediaRouteControllerInternal(kRouteId2, _, _, _))
      .Times(2)
      .WillRepeatedly(Invoke(OnCreateMediaRouteController));

  MockMediaRouteControllerObserver observer1a(
      router()->GetRouteController(kRouteId));
  MockMediaRouteControllerObserver observer2a(
      router()->GetRouteController(kRouteId2));

  // Update the routes list with |kRouteId| but without |kRouteId2|. This should
  // remove the controller for |kRouteId2|, resulting in
  // CreateMediaRouteController() getting called again for |kRouteId2| below.
  router()->OnRoutesUpdated({CreateMediaRoute()}, std::string(),
                            std::vector<std::string>());
  // Add back |kRouteId2| so that a controller can be created for it.
  router()->OnRoutesUpdated({CreateMediaRoute(), CreateMediaRoute2()},
                            std::string(), std::vector<std::string>());

  MockMediaRouteControllerObserver observer1b(
      router()->GetRouteController(kRouteId));
  MockMediaRouteControllerObserver observer2b(
      router()->GetRouteController(kRouteId2));

  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
