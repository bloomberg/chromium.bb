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
#include "base/synchronization/waitable_event.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_test.h"
#include "chrome/browser/media/router/route_message.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/version_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/test_util.h"
#include "extensions/common/value_builder.h"
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
using testing::SaveArg;
using testing::Sequence;

namespace media_router {

namespace {

const char kDescription[] = "description";
const char kError[] = "error";
const char kMessage[] = "message";
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
const uint8_t kBinaryMessage[] = {0x01, 0x02, 0x03, 0x04};
const int kTimeoutMillis = 5 * 1000;

IssueInfo CreateIssueInfo(const std::string& title) {
  IssueInfo issue_info;
  issue_info.title = title;
  issue_info.message = std::string("msg");
  issue_info.default_action = IssueInfo::Action::DISMISS;
  issue_info.severity = IssueInfo::Severity::WARNING;
  return issue_info;
}

MediaRoute CreateMediaRoute() {
  return MediaRoute(kRouteId, MediaSource(kSource), kSinkId, kDescription, true,
                    std::string(), true);
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

class SinkResponseCallbackHandler {
 public:
  MOCK_METHOD1(Invoke, void(const std::string& sink_id));
};

class SendMessageCallbackHandler {
 public:
  MOCK_METHOD1(Invoke, void(bool));
};

template <typename T>
void StoreAndRun(T* result, const base::Closure& closure, const T& result_val) {
  *result = result_val;
  closure.Run();
}

class MediaRouterMojoImplTest : public MediaRouterMojoTest {
 public:
  MediaRouterMojoImplTest() {}
  ~MediaRouterMojoImplTest() override {}

  void ExpectResultBucketCount(const std::string& operation,
                               RouteRequestResult::ResultCode result_code,
                               int expected_count) {
    histogram_tester_.ExpectBucketCount(
        "MediaRouter.Provider." + operation + ".Result",
        result_code,
        expected_count);
  }

 private:
  base::HistogramTester histogram_tester_;
};

// ProcessManager with a mocked method subset, for testing extension suspend
// handling.
class TestProcessManager : public extensions::ProcessManager {
 public:
  explicit TestProcessManager(content::BrowserContext* context)
      : extensions::ProcessManager(
            context,
            context,
            extensions::ExtensionRegistry::Get(context)) {}
  ~TestProcessManager() override {}

  static std::unique_ptr<KeyedService> Create(
      content::BrowserContext* context) {
    return base::MakeUnique<TestProcessManager>(context);
  }

  MOCK_METHOD1(IsEventPageSuspended, bool(const std::string& ext_id));

  MOCK_METHOD2(WakeEventPage,
               bool(const std::string& extension_id,
                    const base::Callback<void(bool)>& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestProcessManager);
};

// Mockable class for awaiting RegisterMediaRouteProvider callbacks.
class RegisterMediaRouteProviderHandler {
 public:
  MOCK_METHOD1(Invoke, void(const std::string& instance_id));
};

TEST_F(MediaRouterMojoImplTest, CreateRoute) {
  MediaSource media_source(kSource);
  MediaRoute expected_route(kRouteId, media_source, kSinkId, "", false, "",
                            false);

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(mock_media_route_provider_,
              CreateRoute(kSource, kSinkId, _, url::Origin(GURL(kOrigin)),
                          kInvalidTabId, _, _, _))
      .WillOnce(Invoke(
          [](const std::string& source, const std::string& sink,
             const std::string& presentation_id, const url::Origin& origin,
             int tab_id, base::TimeDelta timeout, bool incognito,
             const mojom::MediaRouteProvider::CreateRouteCallback& cb) {
            cb.Run(CreateMediaRoute(), std::string(), RouteRequestResult::OK);
          }));

  base::RunLoop run_loop;
  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, DoInvoke(Pointee(Equals(expected_route)), Not(""), "",
                                RouteRequestResult::OK))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->CreateRoute(kSource, kSinkId, url::Origin(GURL(kOrigin)), nullptr,
                        route_response_callbacks,
                        base::TimeDelta::FromMilliseconds(kTimeoutMillis),
                        false);
  run_loop.Run();
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
  EXPECT_CALL(mock_media_route_provider_,
              CreateRoute(kSource, kSinkId, _, url::Origin(GURL(kOrigin)),
                          kInvalidTabId, _, _, _))
      .WillOnce(Invoke([&expected_route](
          const std::string& source, const std::string& sink,
          const std::string& presentation_id, const url::Origin& origin,
          int tab_id, base::TimeDelta timeout, bool incognito,
          const mojom::MediaRouteProvider::CreateRouteCallback& cb) {
        cb.Run(expected_route, std::string(), RouteRequestResult::OK);
      }));

  base::RunLoop run_loop;
  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, DoInvoke(Pointee(Equals(expected_route)), Not(""), "",
                                RouteRequestResult::OK))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->CreateRoute(kSource, kSinkId, url::Origin(GURL(kOrigin)), nullptr,
                        route_response_callbacks,
                        base::TimeDelta::FromMilliseconds(kTimeoutMillis),
                        true);
  run_loop.Run();
  ExpectResultBucketCount("CreateRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, CreateRouteFails) {
  EXPECT_CALL(
      mock_media_route_provider_,
      CreateRoute(kSource, kSinkId, _, url::Origin(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), _, _))
      .WillOnce(Invoke(
          [](const std::string& source, const std::string& sink,
             const std::string& presentation_id, const url::Origin& origin,
             int tab_id, base::TimeDelta timeout, bool incognito,
             const mojom::MediaRouteProvider::CreateRouteCallback& cb) {
            cb.Run(base::nullopt, std::string(kError),
                   RouteRequestResult::TIMED_OUT);
          }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", kError, RouteRequestResult::TIMED_OUT))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->CreateRoute(kSource, kSinkId, url::Origin(GURL(kOrigin)), nullptr,
                        route_response_callbacks,
                        base::TimeDelta::FromMilliseconds(kTimeoutMillis),
                        false);
  run_loop.Run();
  ExpectResultBucketCount("CreateRoute", RouteRequestResult::TIMED_OUT, 1);
}

TEST_F(MediaRouterMojoImplTest, CreateRouteIncognitoMismatchFails) {
  EXPECT_CALL(
      mock_media_route_provider_,
      CreateRoute(kSource, kSinkId, _, url::Origin(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(Invoke(
          [](const std::string& source, const std::string& sink,
             const std::string& presentation_id, const url::Origin& origin,
             int tab_id, base::TimeDelta timeout, bool incognito,
             const mojom::MediaRouteProvider::CreateRouteCallback& cb) {
            cb.Run(CreateMediaRoute(), std::string(), RouteRequestResult::OK);
          }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  std::string error("Mismatch in incognito status: request = 1, response = 0");
  EXPECT_CALL(handler, DoInvoke(nullptr, "", error,
                                RouteRequestResult::INCOGNITO_MISMATCH))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->CreateRoute(kSource, kSinkId, url::Origin(GURL(kOrigin)), nullptr,
                        route_response_callbacks,
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
      CreateRoute(kSource, kSinkId, _, url::Origin(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(Invoke([&route](
          const std::string& source, const std::string& sink,
          const std::string& presentation_id, const url::Origin& origin,
          int tab_id, base::TimeDelta timeout, bool incognito,
          const mojom::MediaRouteProvider::CreateRouteCallback& cb) {
        cb.Run(route, std::string(), RouteRequestResult::OK);
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

  EXPECT_CALL(mock_media_route_provider_, TerminateRoute(kRouteId, _))
      .WillOnce(Invoke(
          [](const std::string& route_id,
             const mojom::MediaRouteProvider::TerminateRouteCallback& cb) {
            cb.Run(base::nullopt, RouteRequestResult::OK);
          }));

  base::RunLoop run_loop2;
  router()->OnIncognitoProfileShutdown();
  run_loop2.RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, JoinRoute) {
  MediaSource media_source(kSource);
  MediaRoute expected_route(kRouteId, media_source, kSinkId, "", false, "",
                            false);

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
      JoinRoute(kSource, kPresentationId, url::Origin(GURL(kOrigin)),
                kInvalidTabId,
                base::TimeDelta::FromMilliseconds(kTimeoutMillis), _, _))
      .WillOnce(Invoke([&route](
          const std::string& source, const std::string& presentation_id,
          const url::Origin& origin, int tab_id, base::TimeDelta timeout,
          bool incognito,
          const mojom::MediaRouteProvider::JoinRouteCallback& cb) {
        cb.Run(route, std::string(), RouteRequestResult::OK);
      }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler, DoInvoke(Pointee(Equals(expected_route)), Not(""), "",
                                RouteRequestResult::OK))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->JoinRoute(kSource, kPresentationId, url::Origin(GURL(kOrigin)),
                      nullptr, route_response_callbacks,
                      base::TimeDelta::FromMilliseconds(kTimeoutMillis), false);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, JoinRouteNotFoundFails) {
  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler, DoInvoke(nullptr, "", "Route not found",
                                RouteRequestResult::ROUTE_NOT_FOUND))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->JoinRoute(kSource, kPresentationId, url::Origin(GURL(kOrigin)),
                      nullptr, route_response_callbacks,
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
      JoinRoute(kSource, kPresentationId, url::Origin(GURL(kOrigin)),
                kInvalidTabId,
                base::TimeDelta::FromMilliseconds(kTimeoutMillis), _, _))
      .WillOnce(Invoke(
          [](const std::string& source, const std::string& presentation_id,
             const url::Origin& origin, int tab_id, base::TimeDelta timeout,
             bool incognito,
             const mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            cb.Run(base::nullopt, std::string(kError),
                   RouteRequestResult::TIMED_OUT);
          }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", kError, RouteRequestResult::TIMED_OUT))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->JoinRoute(kSource, kPresentationId, url::Origin(GURL(kOrigin)),
                      nullptr, route_response_callbacks,
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
      JoinRoute(kSource, kPresentationId, url::Origin(GURL(kOrigin)),
                kInvalidTabId,
                base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(Invoke([&route](
          const std::string& source, const std::string& presentation_id,
          const url::Origin& origin, int tab_id, base::TimeDelta timeout,
          bool incognito,
          const mojom::MediaRouteProvider::JoinRouteCallback& cb) {
        cb.Run(route, std::string(), RouteRequestResult::OK);
      }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  std::string error("Mismatch in incognito status: request = 1, response = 0");
  EXPECT_CALL(handler, DoInvoke(nullptr, "", error,
                                RouteRequestResult::INCOGNITO_MISMATCH))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->JoinRoute(kSource, kPresentationId, url::Origin(GURL(kOrigin)),
                      nullptr, route_response_callbacks,
                      base::TimeDelta::FromMilliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::INCOGNITO_MISMATCH,
                          1);
}

TEST_F(MediaRouterMojoImplTest, ConnectRouteByRouteId) {
  MediaSource media_source(kSource);
  MediaRoute expected_route(kRouteId, media_source, kSinkId, "", false, "",
                            false);
  expected_route.set_incognito(false);
  MediaRoute route = CreateMediaRoute();

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(
      mock_media_route_provider_,
      ConnectRouteByRouteId(
          kSource, kRouteId, _, url::Origin(GURL(kOrigin)), kInvalidTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), false, _))
      .WillOnce(Invoke([&route](
          const std::string& source, const std::string& route_id,
          const std::string& presentation_id, const url::Origin& origin,
          int tab_id, base::TimeDelta timeout, bool incognito,
          const mojom::MediaRouteProvider::JoinRouteCallback& cb) {
        cb.Run(route, std::string(), RouteRequestResult::OK);
      }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler, DoInvoke(Pointee(Equals(expected_route)), Not(""), "",
                                RouteRequestResult::OK))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->ConnectRouteByRouteId(
      kSource, kRouteId, url::Origin(GURL(kOrigin)), nullptr,
      route_response_callbacks,
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), false);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, ConnectRouteByRouteIdFails) {
  EXPECT_CALL(
      mock_media_route_provider_,
      ConnectRouteByRouteId(
          kSource, kRouteId, _, url::Origin(GURL(kOrigin)), kInvalidTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(Invoke(
          [](const std::string& source, const std::string& route_id,
             const std::string& presentation_id, const url::Origin& origin,
             int tab_id, base::TimeDelta timeout, bool incognito,
             const mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            cb.Run(base::nullopt, std::string(kError),
                   RouteRequestResult::TIMED_OUT);
          }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", kError, RouteRequestResult::TIMED_OUT))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->ConnectRouteByRouteId(
      kSource, kRouteId, url::Origin(GURL(kOrigin)), nullptr,
      route_response_callbacks,
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
      ConnectRouteByRouteId(
          kSource, kRouteId, _, url::Origin(GURL(kOrigin)), kInvalidTabId,
          base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(Invoke([&route](
          const std::string& source, const std::string& route_id,
          const std::string& presentation_id, const url::Origin& origin,
          int tab_id, base::TimeDelta timeout, bool incognito,
          const mojom::MediaRouteProvider::JoinRouteCallback& cb) {
        cb.Run(route, std::string(), RouteRequestResult::OK);
      }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  std::string error("Mismatch in incognito status: request = 1, response = 0");
  EXPECT_CALL(handler, DoInvoke(nullptr, "", error,
                                RouteRequestResult::INCOGNITO_MISMATCH))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->ConnectRouteByRouteId(
      kSource, kRouteId, url::Origin(GURL(kOrigin)), nullptr,
      route_response_callbacks,
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::INCOGNITO_MISMATCH,
                          1);
}

TEST_F(MediaRouterMojoImplTest, DetachRoute) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_media_route_provider_, DetachRoute(kRouteId))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->DetachRoute(kRouteId);
  run_loop.Run();
}

TEST_F(MediaRouterMojoImplTest, TerminateRoute) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_media_route_provider_, TerminateRoute(kRouteId, _))
      .WillOnce(Invoke(
          [](const std::string& route_id,
             const mojom::MediaRouteProvider::TerminateRouteCallback& cb) {
            cb.Run(base::nullopt, RouteRequestResult::OK);
          }));
  router()->TerminateRoute(kRouteId);
  run_loop.RunUntilIdle();
  ExpectResultBucketCount("TerminateRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, TerminateRouteFails) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_media_route_provider_, TerminateRoute(kRouteId, _))
      .WillOnce(Invoke(
          [](const std::string& route_id,
             const mojom::MediaRouteProvider::TerminateRouteCallback& cb) {
            cb.Run(std::string("timed out"), RouteRequestResult::TIMED_OUT);
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

  base::RunLoop run_loop;
  media_router_proxy_->OnIssue(issue_info);
  run_loop.RunUntilIdle();

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

  std::unique_ptr<MockMediaSinksObserver> sinks_observer(
      new MockMediaSinksObserver(router(), media_source,
                                 url::Origin(GURL(kOrigin))));
  EXPECT_TRUE(sinks_observer->Init());
  std::unique_ptr<MockMediaSinksObserver> extra_sinks_observer(
      new MockMediaSinksObserver(router(), media_source,
                                 url::Origin(GURL(kOrigin))));
  EXPECT_TRUE(extra_sinks_observer->Init());
  std::unique_ptr<MockMediaSinksObserver> unrelated_sinks_observer(
      new MockMediaSinksObserver(router(), MediaSource(kSource2),
                                 url::Origin(GURL(kOrigin))));
  EXPECT_TRUE(unrelated_sinks_observer->Init());
  ProcessEventLoop();

  std::vector<MediaSink> expected_sinks;
  expected_sinks.push_back(
      MediaSink(kSinkId, kSinkName, MediaSink::IconType::CAST));
  expected_sinks.push_back(
      MediaSink(kSinkId2, kSinkName, MediaSink::IconType::CAST));

  base::RunLoop run_loop;
  EXPECT_CALL(*sinks_observer, OnSinksReceived(SequenceEquals(expected_sinks)));
  EXPECT_CALL(*extra_sinks_observer,
              OnSinksReceived(SequenceEquals(expected_sinks)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  media_router_proxy_->OnSinksReceived(
      media_source.id(), expected_sinks,
      std::vector<url::Origin>(1, url::Origin(GURL(kOrigin))));
  run_loop.Run();

  // Since the MediaRouterMojoImpl has already received results for
  // |media_source|, return cached results to observers that are subsequently
  // registered.
  std::unique_ptr<MockMediaSinksObserver> cached_sinks_observer(
      new MockMediaSinksObserver(router(), media_source,
                                 url::Origin(GURL(kOrigin))));
  EXPECT_CALL(*cached_sinks_observer,
              OnSinksReceived(SequenceEquals(expected_sinks)));
  EXPECT_TRUE(cached_sinks_observer->Init());

  // Different origin from cached result. Empty list will be returned.
  std::unique_ptr<MockMediaSinksObserver> cached_sinks_observer2(
      new MockMediaSinksObserver(router(), media_source,
                                 url::Origin(GURL("https://youtube.com"))));
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
  std::unique_ptr<MockMediaSinksObserver> sinks_observer(
      new MockMediaSinksObserver(router(), media_source,
                                 url::Origin(GURL(kOrigin))));
  EXPECT_CALL(*sinks_observer, OnSinksReceived(IsEmpty()));
  EXPECT_TRUE(sinks_observer->Init());
  MediaSource media_source2(kSource2);
  std::unique_ptr<MockMediaSinksObserver> sinks_observer2(
      new MockMediaSinksObserver(router(), media_source2,
                                 url::Origin(GURL(kOrigin))));
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
  media_router_proxy_->OnRoutesUpdated(expected_routes, media_source.id(),
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
  std::unique_ptr<MockMediaRoutesObserver> observer1(
      new MockMediaRoutesObserver(router(), media_source.id()));
  ProcessEventLoop();
  EXPECT_CALL(*observer1, OnRoutesUpdated(SequenceEquals(expected_routes),
                                          expected_joinable_route_ids))
      .Times(1);
  media_router_proxy_->OnRoutesUpdated(expected_routes, media_source.id(),
                                       expected_joinable_route_ids);
  ProcessEventLoop();

  // Creating two more observers will not wake up the provider. Instead, the
  // cached route list will be returned.
  std::unique_ptr<MockMediaRoutesObserver> observer2(
      new MockMediaRoutesObserver(router(), media_source.id()));
  std::unique_ptr<MockMediaRoutesObserver> observer3(
      new MockMediaRoutesObserver(router(), media_source.id()));
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
  EXPECT_CALL(
      mock_media_route_provider_, SendRouteMessage(kRouteId, kMessage, _))
      .WillOnce(Invoke([](
          const MediaRoute::Id& route_id, const std::string& message,
          const mojom::MediaRouteProvider::SendRouteMessageCallback& cb) {
        cb.Run(true);
      }));

  base::RunLoop run_loop;
  SendMessageCallbackHandler handler;
  EXPECT_CALL(handler, Invoke(true))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->SendRouteMessage(kRouteId, kMessage,
                             base::Bind(&SendMessageCallbackHandler::Invoke,
                                        base::Unretained(&handler)));
  run_loop.Run();
}

TEST_F(MediaRouterMojoImplTest, SendRouteBinaryMessage) {
  std::unique_ptr<std::vector<uint8_t>> expected_binary_data(
      new std::vector<uint8_t>(kBinaryMessage,
                               kBinaryMessage + arraysize(kBinaryMessage)));

  EXPECT_CALL(mock_media_route_provider_,
              SendRouteBinaryMessageInternal(kRouteId, _, _))
      .WillOnce(Invoke([](
          const MediaRoute::Id& route_id, const std::vector<uint8_t>& data,
          const mojom::MediaRouteProvider::SendRouteMessageCallback& cb) {
        EXPECT_EQ(
            0, memcmp(kBinaryMessage, &(data[0]), arraysize(kBinaryMessage)));
        cb.Run(true);
      }));

  base::RunLoop run_loop;
  SendMessageCallbackHandler handler;
  EXPECT_CALL(handler, Invoke(true))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->SendRouteBinaryMessage(
      kRouteId, std::move(expected_binary_data),
      base::Bind(&SendMessageCallbackHandler::Invoke,
                 base::Unretained(&handler)));
  run_loop.Run();
}

namespace {

// Used in the RouteMessages* tests to populate the messages that will be
// processed and dispatched to RouteMessageObservers.
void PopulateRouteMessages(std::vector<RouteMessage>* batch1,
                           std::vector<RouteMessage>* batch2,
                           std::vector<RouteMessage>* batch3) {
  batch1->resize(1);
  batch1->at(0).type = RouteMessage::TEXT;
  batch1->at(0).text = std::string("text1");
  batch2->resize(2);
  batch2->at(0).type = RouteMessage::BINARY;
  batch2->at(0).binary = std::vector<uint8_t>(1, UINT8_C(1));
  batch2->at(1).type = RouteMessage::TEXT;
  batch2->at(1).text = std::string("text2");
  batch3->resize(3);
  batch3->at(0).type = RouteMessage::TEXT;
  batch3->at(0).text = std::string("text3");
  batch3->at(1).type = RouteMessage::BINARY;
  batch3->at(1).binary = std::vector<uint8_t>(1, UINT8_C(2));
  batch3->at(2).type = RouteMessage::BINARY;
  batch3->at(2).binary = std::vector<uint8_t>(1, UINT8_C(3));
}

// Used in the RouteMessages* tests to observe and sanity-check that the
// messages being received from the router are correct and in-sequence. The
// checks here correspond to the expected messages in PopulateRouteMessages()
// above.
class ExpectedMessagesObserver : public RouteMessageObserver {
 public:
  ExpectedMessagesObserver(MediaRouter* router, const MediaRoute::Id& route_id)
      : RouteMessageObserver(router, route_id) {}

  ~ExpectedMessagesObserver() final {
    CheckReceivedMessages();
  }

 private:
  void OnMessagesReceived(
      const std::vector<RouteMessage>& messages) final {
    messages_.insert(messages_.end(), messages.begin(), messages.end());
  }

  void CheckReceivedMessages() {
    ASSERT_EQ(6u, messages_.size());
    EXPECT_EQ(RouteMessage::TEXT, messages_[0].type);
    ASSERT_TRUE(messages_[0].text);
    EXPECT_EQ("text1", *messages_[0].text);
    EXPECT_EQ(RouteMessage::BINARY, messages_[1].type);
    ASSERT_TRUE(messages_[1].binary);
    ASSERT_EQ(1u, messages_[1].binary->size());
    EXPECT_EQ(UINT8_C(1), messages_[1].binary->front());
    EXPECT_EQ(RouteMessage::TEXT, messages_[2].type);
    ASSERT_TRUE(messages_[2].text);
    EXPECT_EQ("text2", *messages_[2].text);
    EXPECT_EQ(RouteMessage::TEXT, messages_[3].type);
    ASSERT_TRUE(messages_[3].text);
    EXPECT_EQ("text3", *messages_[3].text);
    EXPECT_EQ(RouteMessage::BINARY, messages_[4].type);
    ASSERT_TRUE(messages_[4].binary);
    ASSERT_EQ(1u, messages_[4].binary->size());
    EXPECT_EQ(UINT8_C(2), messages_[4].binary->front());
    EXPECT_EQ(RouteMessage::BINARY, messages_[5].type);
    ASSERT_TRUE(messages_[5].binary);
    ASSERT_EQ(1u, messages_[5].binary->size());
    EXPECT_EQ(UINT8_C(3), messages_[5].binary->front());
  }

  std::vector<RouteMessage> messages_;
};

}  // namespace

TEST_F(MediaRouterMojoImplTest, RouteMessagesSingleObserver) {
  std::vector<RouteMessage> incoming_batch1, incoming_batch2, incoming_batch3;
  PopulateRouteMessages(&incoming_batch1, &incoming_batch2, &incoming_batch3);

  base::RunLoop run_loop;
  MediaRoute::Id expected_route_id("foo");
  EXPECT_CALL(mock_media_route_provider_,
              StartListeningForRouteMessages(Eq(expected_route_id)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // Creating ExpectedMessagesObserver will register itself to the
  // MediaRouter, which in turn will start listening for route messages.
  ExpectedMessagesObserver observer(router(), expected_route_id);
  run_loop.Run();  // Will quit when StartListeningForRouteMessages() is called.
  router()->OnRouteMessagesReceived(expected_route_id, incoming_batch1);
  router()->OnRouteMessagesReceived(expected_route_id, incoming_batch2);
  router()->OnRouteMessagesReceived(expected_route_id, incoming_batch3);
  // When |observer| goes out-of-scope, its destructor will ensure all expected
  // messages have been received.
}

TEST_F(MediaRouterMojoImplTest, RouteMessagesMultipleObservers) {
  std::vector<RouteMessage> incoming_batch1, incoming_batch2, incoming_batch3;
  PopulateRouteMessages(&incoming_batch1, &incoming_batch2, &incoming_batch3);

  base::RunLoop run_loop;
  MediaRoute::Id expected_route_id("foo");
  EXPECT_CALL(mock_media_route_provider_,
              StartListeningForRouteMessages(Eq(expected_route_id)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // The ExpectedMessagesObservers will register themselves with the
  // MediaRouter, which in turn will start listening for route messages.
  ExpectedMessagesObserver observer1(router(), expected_route_id);
  ExpectedMessagesObserver observer2(router(), expected_route_id);
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
  content::PresentationSessionInfo connection(presentation_url,
                                              kPresentationId);
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
    media_router_proxy_->OnPresentationConnectionClosed(
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
    media_router_proxy_->OnPresentationConnectionStateChanged(
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
  media_router_proxy_->OnPresentationConnectionStateChanged(
      route_id, content::PRESENTATION_CONNECTION_STATE_TERMINATED);
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, QueuedWhileAsleep) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_event_page_tracker_, IsEventPageSuspended(extension_id()))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_event_page_tracker_, WakeEventPage(extension_id(), _))
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(DoAll(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }),
                      Return(true)));
  router()->DetachRoute(kRouteId);
  router()->DetachRoute(kRouteId2);
  run_loop.Run();
  EXPECT_CALL(mock_event_page_tracker_, IsEventPageSuspended(extension_id()))
      .Times(2)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_media_route_provider_, DetachRoute(kRouteId));
  EXPECT_CALL(mock_media_route_provider_, DetachRoute(kRouteId2));
  ConnectProviderManagerService();
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, SearchSinks) {
  std::string search_input("input");
  std::string domain("google.com");
  MediaSource media_source(kSource);

  EXPECT_CALL(
      mock_media_route_provider_, SearchSinks_(kSinkId, kSource, _, _))
      .WillOnce(Invoke([&search_input, &domain](
          const std::string& sink_id, const std::string& source,
          const mojom::SinkSearchCriteriaPtr& search_criteria,
          const mojom::MediaRouteProvider::SearchSinksCallback& cb) {
        EXPECT_EQ(search_input, search_criteria->input);
        EXPECT_EQ(domain, search_criteria->domain);
        cb.Run(kSinkId2);
      }));

  SinkResponseCallbackHandler sink_handler;
  EXPECT_CALL(sink_handler, Invoke(kSinkId2)).Times(1);
  MediaSinkSearchResponseCallback sink_callback = base::Bind(
      &SinkResponseCallbackHandler::Invoke, base::Unretained(&sink_handler));

  router()->SearchSinks(kSinkId, kSource, search_input, domain, sink_callback);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

class MediaRouterMojoExtensionTest : public ::testing::Test {
 public:
  MediaRouterMojoExtensionTest() : process_manager_(nullptr) {}

  ~MediaRouterMojoExtensionTest() override {}

 protected:
  void SetUp() override {
    // Set the extension's version number to be identical to the browser's.
    extension_ =
        extensions::test_util::BuildExtension(extensions::ExtensionBuilder())
            .MergeManifest(extensions::DictionaryBuilder()
                               .Set("version", version_info::GetVersionNumber())
                               .Build())
            .Build();

    profile_.reset(new TestingProfile);
    // Set up a mock ProcessManager instance.
    extensions::ProcessManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(), &TestProcessManager::Create);
    process_manager_ = static_cast<TestProcessManager*>(
        extensions::ProcessManager::Get(profile_.get()));
    DCHECK(process_manager_);

    // Create MR and its proxy, so that it can be accessed through Mojo.
    media_router_.reset(new MediaRouterMojoImpl(process_manager_));
    ProcessEventLoop();
  }

  void TearDown() override {
    media_router_.reset();
    profile_.reset();
  }

  // Constructs bindings so that |media_router_| delegates calls to
  // |mojo_media_router_|, which are then handled by
  // |mock_media_route_provider_service_|.
  void BindMediaRouteProvider() {
    binding_.reset(new mojo::Binding<mojom::MediaRouteProvider>(
        &mock_media_route_provider_,
        mojo::MakeRequest(&media_route_provider_proxy_)));
    media_router_->BindToMojoRequest(mojo::MakeRequest(&media_router_proxy_),
                                     *extension_);
  }

  void ResetMediaRouteProvider() {
    binding_.reset();
    media_router_->BindToMojoRequest(mojo::MakeRequest(&media_router_proxy_),
                                     *extension_);
  }

  void RegisterMediaRouteProvider() {
    media_router_proxy_->RegisterMediaRouteProvider(
        std::move(media_route_provider_proxy_),
        base::Bind(&RegisterMediaRouteProviderHandler::Invoke,
                   base::Unretained(&provide_handler_)));
  }

  void ProcessEventLoop() { base::RunLoop().RunUntilIdle(); }

  void ExpectWakeReasonBucketCount(MediaRouteProviderWakeReason reason,
                                   int expected_count) {
    histogram_tester_.ExpectBucketCount("MediaRouter.Provider.WakeReason",
                                        static_cast<int>(reason),
                                        expected_count);
  }

  void ExpectVersionBucketCount(MediaRouteProviderVersion version,
                                int expected_count) {
    histogram_tester_.ExpectBucketCount("MediaRouter.Provider.Version",
                                        static_cast<int>(version),
                                        expected_count);
  }

  void ExpectWakeupBucketCount(MediaRouteProviderWakeup wakeup,
                               int expected_count) {
    histogram_tester_.ExpectBucketCount("MediaRouter.Provider.Wakeup",
                                        static_cast<int>(wakeup),
                                        expected_count);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<MediaRouterMojoImpl> media_router_;
  RegisterMediaRouteProviderHandler provide_handler_;
  TestProcessManager* process_manager_;
  testing::StrictMock<MockMediaRouteProvider> mock_media_route_provider_;
  mojom::MediaRouterPtr media_router_proxy_;
  scoped_refptr<extensions::Extension> extension_;

 private:
  std::unique_ptr<TestingProfile> profile_;
  mojom::MediaRouteProviderPtr media_route_provider_proxy_;
  std::unique_ptr<mojo::Binding<mojom::MediaRouteProvider>> binding_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterMojoExtensionTest);
};

TEST_F(MediaRouterMojoExtensionTest, DeferredBindingAndSuspension) {
  // DetachRoute is called before *any* extension has connected.
  // It should be queued.
  media_router_->DetachRoute(kRouteId);

  BindMediaRouteProvider();

  base::RunLoop run_loop, run_loop2;
  // |mojo_media_router| signals its readiness to the MR by registering
  // itself via RegisterMediaRouteProvider().
  // Now that the |media_router| and |mojo_media_router| are fully initialized,
  // the queued DetachRoute() call should be executed.
  EXPECT_CALL(provide_handler_, Invoke(testing::Not("")))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_media_route_provider_, EnableMdnsDiscovery())
      .Times(AtMost(1));
  EXPECT_CALL(mock_media_route_provider_, DetachRoute(kRouteId))
      .WillOnce(InvokeWithoutArgs([&run_loop2]() { run_loop2.Quit(); }));
  RegisterMediaRouteProvider();
  run_loop.Run();
  run_loop2.Run();

  base::RunLoop run_loop3;
  // Extension is suspended and re-awoken.
  ResetMediaRouteProvider();
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()))
      .WillOnce(Return(true));
  EXPECT_CALL(*process_manager_, WakeEventPage(extension_->id(), _))
      .WillOnce(testing::DoAll(
          media::RunCallback<1>(true),
          InvokeWithoutArgs([&run_loop3]() { run_loop3.Quit(); }),
          Return(true)));
  media_router_->DetachRoute(kRouteId2);
  run_loop3.Run();

  base::RunLoop run_loop4, run_loop5;
  // RegisterMediaRouteProvider() is called.
  // The queued DetachRoute(kRouteId2) call should be executed.
  EXPECT_CALL(provide_handler_, Invoke(testing::Not("")))
      .WillOnce(InvokeWithoutArgs([&run_loop4]() { run_loop4.Quit(); }));
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_media_route_provider_, EnableMdnsDiscovery())
      .Times(AtMost(1));
  EXPECT_CALL(mock_media_route_provider_, DetachRoute(kRouteId2))
      .WillOnce(InvokeWithoutArgs([&run_loop5]() { run_loop5.Quit(); }));
  BindMediaRouteProvider();
  RegisterMediaRouteProvider();
  run_loop4.Run();
  run_loop5.Run();
  ExpectWakeReasonBucketCount(MediaRouteProviderWakeReason::DETACH_ROUTE, 1);
  ExpectWakeupBucketCount(MediaRouteProviderWakeup::SUCCESS, 1);
  ExpectVersionBucketCount(MediaRouteProviderVersion::SAME_VERSION_AS_CHROME,
                           1);
}

TEST_F(MediaRouterMojoExtensionTest, AttemptedWakeupTooManyTimes) {
  BindMediaRouteProvider();

  // DetachRoute is called while extension is suspended. It should be queued.
  // Schedule a component extension wakeup.
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()))
      .WillOnce(Return(true));
  EXPECT_CALL(*process_manager_, WakeEventPage(extension_->id(), _))
      .WillOnce(testing::DoAll(media::RunCallback<1>(true), Return(true)));
  media_router_->DetachRoute(kRouteId);
  EXPECT_EQ(1u, media_router_->pending_requests_.size());
  ExpectWakeReasonBucketCount(MediaRouteProviderWakeReason::DETACH_ROUTE, 1);
  ExpectWakeupBucketCount(MediaRouteProviderWakeup::SUCCESS, 1);

  // Media route provider fails to connect to media router before extension is
  // suspended again, and |OnConnectionError| is invoked. Retry the wakeup.
  EXPECT_CALL(*process_manager_, WakeEventPage(extension_->id(), _))
      .Times(MediaRouterMojoImpl::kMaxWakeupAttemptCount - 1)
      .WillRepeatedly(
          testing::DoAll(media::RunCallback<1>(true), Return(true)));
  for (int i = 0; i < MediaRouterMojoImpl::kMaxWakeupAttemptCount - 1; ++i)
    media_router_->OnConnectionError();

  // We have already tried |kMaxWakeupAttemptCount| times. If we get an error
  // again, we will give up and the pending request queue will be drained.
  media_router_->OnConnectionError();
  EXPECT_TRUE(media_router_->pending_requests_.empty());
  ExpectWakeReasonBucketCount(MediaRouteProviderWakeReason::CONNECTION_ERROR,
                              MediaRouterMojoImpl::kMaxWakeupAttemptCount - 1);
  ExpectWakeupBucketCount(MediaRouteProviderWakeup::ERROR_TOO_MANY_RETRIES, 1);

  // Requests that comes in after queue is drained should be queued.
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()))
      .WillOnce(Return(true));
  EXPECT_CALL(*process_manager_, WakeEventPage(extension_->id(), _))
      .WillOnce(testing::DoAll(media::RunCallback<1>(true), Return(true)));
  media_router_->DetachRoute(kRouteId);
  EXPECT_EQ(1u, media_router_->pending_requests_.size());
  ExpectVersionBucketCount(MediaRouteProviderVersion::SAME_VERSION_AS_CHROME,
                           1);
}

TEST_F(MediaRouterMojoExtensionTest, WakeupFailedDrainsQueue) {
  BindMediaRouteProvider();

  // DetachRoute is called while extension is suspended. It should be queued.
  // Schedule a component extension wakeup.
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()))
      .WillOnce(Return(true));
  base::Callback<void(bool)> extension_wakeup_callback;
  EXPECT_CALL(*process_manager_, WakeEventPage(extension_->id(), _))
      .WillOnce(
          testing::DoAll(SaveArg<1>(&extension_wakeup_callback), Return(true)));
  media_router_->DetachRoute(kRouteId);
  EXPECT_EQ(1u, media_router_->pending_requests_.size());

  // Extension wakeup callback returning false is an non-retryable error.
  // Queue should be drained.
  extension_wakeup_callback.Run(false);
  EXPECT_TRUE(media_router_->pending_requests_.empty());

  // Requests that comes in after queue is drained should be queued.
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()))
      .WillOnce(Return(true));
  EXPECT_CALL(*process_manager_, WakeEventPage(extension_->id(), _))
      .WillOnce(testing::DoAll(media::RunCallback<1>(true), Return(true)));
  media_router_->DetachRoute(kRouteId);
  EXPECT_EQ(1u, media_router_->pending_requests_.size());
  ExpectWakeReasonBucketCount(MediaRouteProviderWakeReason::DETACH_ROUTE, 1);
  ExpectWakeupBucketCount(MediaRouteProviderWakeup::ERROR_UNKNOWN, 1);
  ExpectVersionBucketCount(MediaRouteProviderVersion::SAME_VERSION_AS_CHROME,
                           1);
}

TEST_F(MediaRouterMojoExtensionTest, DropOldestPendingRequest) {
  const size_t kMaxPendingRequests = MediaRouterMojoImpl::kMaxPendingRequests;

  // Request is queued.
  media_router_->DetachRoute(kRouteId);
  EXPECT_EQ(1u, media_router_->pending_requests_.size());

  for (size_t i = 0; i < kMaxPendingRequests; ++i)
    media_router_->DetachRoute(kRouteId2);

  // The request queue size should not exceed |kMaxPendingRequests|.
  EXPECT_EQ(kMaxPendingRequests, media_router_->pending_requests_.size());

  base::RunLoop run_loop, run_loop2;
  size_t count = 0;
  // The oldest request should have been dropped, so we don't expect to see
  // DetachRoute(kRouteId) here.
  BindMediaRouteProvider();
  EXPECT_CALL(provide_handler_, Invoke(testing::Not("")))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()));
  EXPECT_CALL(mock_media_route_provider_, EnableMdnsDiscovery())
      .Times(AtMost(1));
  EXPECT_CALL(mock_media_route_provider_, DetachRoute(kRouteId2))
      .Times(kMaxPendingRequests)
      .WillRepeatedly(InvokeWithoutArgs([&run_loop2, &count]() {
        if (++count == MediaRouterMojoImpl::kMaxPendingRequests)
          run_loop2.Quit();
      }));
  RegisterMediaRouteProvider();
  run_loop.Run();
  run_loop2.Run();
  ExpectVersionBucketCount(MediaRouteProviderVersion::SAME_VERSION_AS_CHROME,
                           1);
}

#if defined(OS_WIN)
TEST_F(MediaRouterMojoExtensionTest, EnableMdnsAfterEachRegister) {
  // This should be queued since no MRPM is registered yet.
  media_router_->OnUserGesture();

  BindMediaRouteProvider();

  base::RunLoop run_loop;
  base::RunLoop run_loop2;
  EXPECT_CALL(provide_handler_, Invoke(testing::Not("")))
      .WillOnce(InvokeWithoutArgs([&run_loop]() {
                  run_loop.Quit();
                }));
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()))
      .WillOnce(Return(false)).WillOnce(Return(false));
  EXPECT_CALL(mock_media_route_provider_,
              UpdateMediaSinks(MediaSourceForDesktop().id()))
      .Times(2);
  // EnableMdnsDisocvery() is never called except on Windows.
  EXPECT_CALL(mock_media_route_provider_, EnableMdnsDiscovery())
      .WillOnce(InvokeWithoutArgs([&run_loop2]() {
                  run_loop2.Quit();
                }));
  RegisterMediaRouteProvider();
  run_loop.Run();
  run_loop2.Run();
  // Should not call EnableMdnsDiscovery, but will call UpdateMediaSinks
  media_router_->OnUserGesture();
  base::RunLoop run_loop3;
  run_loop3.RunUntilIdle();

  // Reset the extension by "suspending" and notifying MR.
  base::RunLoop run_loop4;
  ResetMediaRouteProvider();
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()))
      .WillOnce(Return(true));
  EXPECT_CALL(*process_manager_, WakeEventPage(extension_->id(), _))
      .WillOnce(testing::DoAll(
          media::RunCallback<1>(true),
          InvokeWithoutArgs([&run_loop4]() { run_loop4.Quit(); }),
          Return(true)));
  // Use DetachRoute because it unconditionally calls RunOrDefer().
  media_router_->DetachRoute(kRouteId);
  run_loop4.Run();

  base::RunLoop run_loop5;
  base::RunLoop run_loop6;
  // RegisterMediaRouteProvider() is called.
  // The queued DetachRoute(kRouteId) call should be executed.
  EXPECT_CALL(provide_handler_, Invoke(testing::Not("")))
      .WillOnce(InvokeWithoutArgs([&run_loop5]() {
                  run_loop5.Quit();
                }));
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()))
      .WillOnce(Return(false)).WillOnce(Return(false));
  // Expected because it was used to wake up the page.
  EXPECT_CALL(mock_media_route_provider_, DetachRoute(kRouteId));
  EXPECT_CALL(mock_media_route_provider_,
              UpdateMediaSinks(MediaSourceForDesktop().id()));
  // EnableMdnsDisocvery() is never called except on Windows.
  EXPECT_CALL(mock_media_route_provider_, EnableMdnsDiscovery())
      .WillOnce(InvokeWithoutArgs([&run_loop6]() {
                  run_loop6.Quit();
                }));
  BindMediaRouteProvider();
  RegisterMediaRouteProvider();
  run_loop5.Run();
  run_loop6.Run();
  // Should not call EnableMdnsDiscovery, but will call UpdateMediaSinks
  media_router_->OnUserGesture();
  base::RunLoop run_loop7;
  run_loop7.RunUntilIdle();
}
#endif

TEST_F(MediaRouterMojoExtensionTest, UpdateMediaSinksOnUserGesture) {
  BindMediaRouteProvider();

  base::RunLoop run_loop;
  EXPECT_CALL(provide_handler_, Invoke(testing::Not("")))
      .WillOnce(InvokeWithoutArgs([&run_loop]() {
                  run_loop.Quit();
                }));
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(extension_->id()))
#if defined(OS_WIN)
      // Windows calls once for EnableMdnsDiscovery
      .Times(3)
#else
      // All others call once for registration, and once for the user gesture.
      .Times(2)
#endif
      .WillRepeatedly(Return(false));


  RegisterMediaRouteProvider();
  run_loop.Run();

  media_router_->OnUserGesture();

  base::RunLoop run_loop2;

#if defined(OS_WIN)
  EXPECT_CALL(mock_media_route_provider_, EnableMdnsDiscovery());
#endif
  EXPECT_CALL(mock_media_route_provider_,
              UpdateMediaSinks(MediaSourceForDesktop().id()))
      .WillOnce(InvokeWithoutArgs([&run_loop2]() {
        run_loop2.Quit();
      }));

  run_loop2.Run();
}

}  // namespace media_router
