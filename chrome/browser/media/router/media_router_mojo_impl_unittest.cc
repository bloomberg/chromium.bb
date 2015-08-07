// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_router_mojo_test.h"
#include "chrome/browser/media/router/media_router_type_converters.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/presentation_session_messages_observer.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "media/base/gmock_callback_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Mock;
using testing::Not;
using testing::Pointee;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;

namespace media_router {

namespace {

const char kDescription[] = "description";
const char kError[] = "error";
const char kExtensionId[] = "extension1234";
const char kMessage[] = "message";
const char kSource[] = "source1";
const char kSource2[] = "source2";
const char kRouteId[] = "routeId";
const char kRouteId2[] = "routeId2";
const char kSink[] = "sink";
const char kSink2[] = "sink2";
const char kSinkName[] = "sinkName";
const char kPresentationId[] = "presentationId";
const char kOrigin[] = "http://origin/";
const int kTabId = 123;
const uint8 kBinaryMessage[] = {0x01, 0x02, 0x03, 0x04};

bool ArePresentationSessionMessagesEqual(
    const content::PresentationSessionMessage* expected,
    const content::PresentationSessionMessage* actual) {
  if (expected->type != actual->type)
    return false;

  return expected->is_binary() ? *expected->data == *actual->data
                               : expected->message == actual->message;
}

interfaces::IssuePtr CreateMojoIssue(const std::string& title) {
  interfaces::IssuePtr mojoIssue = interfaces::Issue::New();
  mojoIssue->title = title;
  mojoIssue->message = "msg";
  mojoIssue->route_id = "";
  mojoIssue->default_action = interfaces::Issue::ActionType::ACTION_TYPE_OK;
  mojoIssue->secondary_actions =
      mojo::Array<interfaces::Issue::ActionType>::New(0);
  mojoIssue->severity = interfaces::Issue::Severity::SEVERITY_WARNING;
  mojoIssue->is_blocking = false;
  mojoIssue->help_url = "";
  return mojoIssue.Pass();
}

}  // namespace

class RouteResponseCallbackHandler {
 public:
  MOCK_METHOD3(Invoke,
               void(const MediaRoute* route,
                    const std::string& presentation_id,
                    const std::string& error_text));
};

class SendMessageCallbackHandler {
 public:
  MOCK_METHOD1(Invoke, void(bool));
};

class ListenForMessagesCallbackHandler {
 public:
  ListenForMessagesCallbackHandler(
      ScopedVector<content::PresentationSessionMessage> expected_messages)
      : expected_messages_(expected_messages.Pass()) {}
  void Invoke(
      const ScopedVector<content::PresentationSessionMessage>& messages) {
    InvokeObserver();
    EXPECT_EQ(messages.size(), expected_messages_.size());
    for (size_t i = 0; i < expected_messages_.size(); ++i) {
      EXPECT_TRUE(ArePresentationSessionMessagesEqual(expected_messages_[i],
                                                      messages[i]));
    }
  }

  MOCK_METHOD0(InvokeObserver, void());

 private:
  ScopedVector<content::PresentationSessionMessage> expected_messages_;
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

  static scoped_ptr<KeyedService> Create(content::BrowserContext* context) {
    return make_scoped_ptr(new TestProcessManager(context));
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
  MediaRoute expected_route(kRouteId, MediaSource(std::string(kSource)),
                            MediaSink(kSink, kSinkName), "", false, "");
  interfaces::MediaRoutePtr route = interfaces::MediaRoute::New();
  route->media_source = kSource;
  route->media_sink = interfaces::MediaSink::New();
  route->media_sink->sink_id = kSink;
  route->media_sink->name = kSinkName;
  route->media_route_id = kRouteId;
  route->description = kDescription;

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(mock_media_route_provider_,
              CreateRoute(mojo::String(kSource), mojo::String(kSink), _,
                          mojo::String(kOrigin), kTabId, _))
      .WillOnce(Invoke([&route](
          const mojo::String& source, const mojo::String& sink,
          const mojo::String& presentation_id, const mojo::String& origin,
          int tab_id,
          const interfaces::MediaRouteProvider::CreateRouteCallback& cb) {
        cb.Run(route.Pass(), mojo::String());
      }));

  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, Invoke(Pointee(Equals(expected_route)), Not(""), ""));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->CreateRoute(kSource, kSink, GURL(kOrigin), kTabId,
                        route_response_callbacks);
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, CreateRouteFails) {
  EXPECT_CALL(mock_media_route_provider_,
              CreateRoute(mojo::String(kSource), mojo::String(kSink), _,
                          mojo::String(kOrigin), kTabId, _))
      .WillOnce(Invoke(
          [](const mojo::String& source, const mojo::String& sink,
             const mojo::String& presentation_id, const mojo::String& origin,
             int tab_id,
             const interfaces::MediaRouteProvider::CreateRouteCallback& cb) {
            cb.Run(interfaces::MediaRoutePtr(), mojo::String(kError));
          }));

  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, Invoke(nullptr, "", kError));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->CreateRoute(kSource, kSink, GURL(kOrigin), kTabId,
                        route_response_callbacks);
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, JoinRoute) {
  MediaRoute expected_route(kRouteId, MediaSource(std::string(kSource)),
                            MediaSink(kSink, kSinkName), "", false, "");
  interfaces::MediaRoutePtr route = interfaces::MediaRoute::New();
  route->media_source = kSource;
  route->media_sink = interfaces::MediaSink::New();
  route->media_sink->sink_id = kSink;
  route->media_sink->name = kSinkName;
  route->media_route_id = kRouteId;
  route->description = kDescription;

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(mock_media_route_provider_,
              JoinRoute(mojo::String(kSource), mojo::String(kPresentationId),
                        mojo::String(kOrigin), kTabId, _))
      .WillOnce(Invoke([&route](
          const mojo::String& source, const mojo::String& presentation_id,
          const mojo::String& origin, int tab_id,
          const interfaces::MediaRouteProvider::JoinRouteCallback& cb) {
        cb.Run(route.Pass(), mojo::String());
      }));

  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, Invoke(Pointee(Equals(expected_route)), Not(""), ""));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->JoinRoute(kSource, kPresentationId, GURL(kOrigin), kTabId,
                      route_response_callbacks);
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, JoinRouteFails) {
  EXPECT_CALL(mock_media_route_provider_,
              JoinRoute(mojo::String(kSource), mojo::String(kPresentationId),
                        mojo::String(kOrigin), kTabId, _))
      .WillOnce(Invoke(
          [](const mojo::String& source, const mojo::String& presentation_id,
             const mojo::String& origin, int tab_id,
             const interfaces::MediaRouteProvider::JoinRouteCallback& cb) {
            cb.Run(interfaces::MediaRoutePtr(), mojo::String(kError));
          }));

  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, Invoke(nullptr, "", kError));
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &RouteResponseCallbackHandler::Invoke, base::Unretained(&handler)));
  router()->JoinRoute(kSource, kPresentationId, GURL(kOrigin), kTabId,
                      route_response_callbacks);
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, CloseRoute) {
  EXPECT_CALL(mock_media_route_provider_, CloseRoute(mojo::String(kRouteId)));
  router()->CloseRoute(kRouteId);
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, HandleIssue) {
  MockIssuesObserver issue_observer1(router());
  MockIssuesObserver issue_observer2(router());
  interfaces::IssuePtr mojo_issue1 = CreateMojoIssue("title 1");
  const Issue& expected_issue1 = mojo_issue1.To<Issue>();

  const Issue* issue;
  EXPECT_CALL(issue_observer1,
              OnIssueUpdated(Pointee(EqualsIssue(expected_issue1))))
      .WillOnce(SaveArg<0>(&issue));
  EXPECT_CALL(issue_observer2,
              OnIssueUpdated(Pointee(EqualsIssue(expected_issue1))));
  media_router_proxy_->OnIssue(mojo_issue1.Pass());
  ProcessEventLoop();

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&issue_observer1));
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&issue_observer2));

  router()->ClearIssue(issue->id());
  router()->UnregisterIssuesObserver(&issue_observer1);
  interfaces::IssuePtr mojo_issue2 = CreateMojoIssue("title 2");
  const Issue& expected_issue2 = mojo_issue2.To<Issue>();

  EXPECT_CALL(issue_observer2,
              OnIssueUpdated(Pointee(EqualsIssue(expected_issue2))));
  media_router_proxy_->OnIssue(mojo_issue2.Pass());
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, RegisterAndUnregisterMediaSinksObserver) {
  MediaSource media_source(kSource);

  MockMediaRouter mock_router;
  EXPECT_CALL(mock_media_route_provider_,
              StartObservingMediaSinks(mojo::String(kSource))).Times(2);
  EXPECT_CALL(mock_media_route_provider_,
              StartObservingMediaSinks(mojo::String(kSource2)));

  MediaSinksObserver* captured_observer;
  EXPECT_CALL(mock_router, RegisterMediaSinksObserver(_))
      .Times(3)
      .WillRepeatedly(SaveArg<0>(&captured_observer));

  MockMediaSinksObserver sinks_observer(&mock_router, media_source);
  EXPECT_EQ(&sinks_observer, captured_observer);
  router()->RegisterMediaSinksObserver(&sinks_observer);
  MockMediaSinksObserver extra_sinks_observer(&mock_router, media_source);
  EXPECT_EQ(&extra_sinks_observer, captured_observer);
  router()->RegisterMediaSinksObserver(&extra_sinks_observer);
  MockMediaSinksObserver unrelated_sinks_observer(&mock_router,
                                                  MediaSource(kSource2));
  EXPECT_EQ(&unrelated_sinks_observer, captured_observer);
  router()->RegisterMediaSinksObserver(&unrelated_sinks_observer);

  std::vector<MediaSink> expected_sinks;
  expected_sinks.push_back(MediaSink(kSink, kSinkName));
  expected_sinks.push_back(MediaSink(kSink2, kSinkName));

  mojo::Array<interfaces::MediaSinkPtr> mojo_sinks(2);
  mojo_sinks[0] = interfaces::MediaSink::New();
  mojo_sinks[0]->sink_id = kSink;
  mojo_sinks[0]->name = kSink;
  mojo_sinks[1] = interfaces::MediaSink::New();
  mojo_sinks[1]->sink_id = kSink2;
  mojo_sinks[1]->name = kSink2;

  EXPECT_CALL(sinks_observer, OnSinksReceived(SequenceEquals(expected_sinks)));
  EXPECT_CALL(extra_sinks_observer,
              OnSinksReceived(SequenceEquals(expected_sinks)));
  media_router_proxy_->OnSinksReceived(media_source.id(), mojo_sinks.Pass());
  ProcessEventLoop();

  EXPECT_CALL(mock_router, UnregisterMediaSinksObserver(&sinks_observer));
  EXPECT_CALL(mock_router, UnregisterMediaSinksObserver(&extra_sinks_observer));
  EXPECT_CALL(mock_router,
              UnregisterMediaSinksObserver(&unrelated_sinks_observer));
  EXPECT_CALL(mock_media_route_provider_,
              StopObservingMediaSinks(mojo::String(kSource)));
  EXPECT_CALL(mock_media_route_provider_,
              StopObservingMediaSinks(mojo::String(kSource2)));
  router()->UnregisterMediaSinksObserver(&sinks_observer);
  router()->UnregisterMediaSinksObserver(&extra_sinks_observer);
  router()->UnregisterMediaSinksObserver(&unrelated_sinks_observer);
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, RegisterAndUnregisterMediaRoutesObserver) {
  MockMediaRouter mock_router;
  EXPECT_CALL(mock_media_route_provider_, StartObservingMediaRoutes()).Times(2);

  MediaRoutesObserver* observer_captured;
  EXPECT_CALL(mock_router, RegisterMediaRoutesObserver(_))
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&observer_captured));
  MockMediaRoutesObserver routes_observer(&mock_router);
  EXPECT_EQ(observer_captured, &routes_observer);
  MockMediaRoutesObserver extra_routes_observer(&mock_router);
  EXPECT_EQ(observer_captured, &extra_routes_observer);
  router()->RegisterMediaRoutesObserver(&routes_observer);
  router()->RegisterMediaRoutesObserver(&extra_routes_observer);

  std::vector<MediaRoute> expected_routes;
  expected_routes.push_back(MediaRoute(kRouteId, MediaSource(kSource),
                                       MediaSink(kSink, kSink), kDescription,
                                       false, ""));
  expected_routes.push_back(MediaRoute(kRouteId2, MediaSource(kSource),
                                       MediaSink(kSink, kSink), kDescription,
                                       false, ""));

  mojo::Array<interfaces::MediaRoutePtr> mojo_routes(2);
  mojo_routes[0] = interfaces::MediaRoute::New();
  mojo_routes[0]->media_route_id = kRouteId;
  mojo_routes[0]->media_source = kSource;
  mojo_routes[0]->media_sink = interfaces::MediaSink::New();
  mojo_routes[0]->media_sink->sink_id = kSink;
  mojo_routes[0]->media_sink->name = kSink;
  mojo_routes[0]->description = kDescription;
  mojo_routes[0]->is_local = false;
  mojo_routes[1] = interfaces::MediaRoute::New();
  mojo_routes[1]->media_route_id = kRouteId2;
  mojo_routes[1]->media_source = kSource;
  mojo_routes[1]->media_sink = interfaces::MediaSink::New();
  mojo_routes[1]->media_sink->sink_id = kSink;
  mojo_routes[1]->media_sink->name = kSink;
  mojo_routes[1]->description = kDescription;
  mojo_routes[1]->is_local = false;

  EXPECT_CALL(routes_observer,
              OnRoutesUpdated(SequenceEquals(expected_routes)));
  EXPECT_CALL(extra_routes_observer,
              OnRoutesUpdated(SequenceEquals(expected_routes)));
  media_router_proxy_->OnRoutesUpdated(mojo_routes.Pass());
  ProcessEventLoop();

  EXPECT_CALL(mock_router, UnregisterMediaRoutesObserver(&routes_observer));
  EXPECT_CALL(mock_router,
              UnregisterMediaRoutesObserver(&extra_routes_observer));
  router()->UnregisterMediaRoutesObserver(&routes_observer);
  router()->UnregisterMediaRoutesObserver(&extra_routes_observer);
  EXPECT_CALL(mock_media_route_provider_, StopObservingMediaRoutes());
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, SendRouteMessage) {
  EXPECT_CALL(
      mock_media_route_provider_,
      SendRouteMessage(mojo::String(kRouteId), mojo::String(kMessage), _))
      .WillOnce(Invoke([](
          const MediaRoute::Id& route_id, const std::string& message,
          const interfaces::MediaRouteProvider::SendRouteMessageCallback& cb) {
        cb.Run(true);
      }));

  SendMessageCallbackHandler handler;
  EXPECT_CALL(handler, Invoke(true));
  router()->SendRouteMessage(kRouteId, kMessage,
                             base::Bind(&SendMessageCallbackHandler::Invoke,
                                        base::Unretained(&handler)));
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, SendRouteBinaryMessage) {
  scoped_ptr<std::vector<uint8>> expected_binary_data(new std::vector<uint8>(
      kBinaryMessage, kBinaryMessage + arraysize(kBinaryMessage)));

  EXPECT_CALL(mock_media_route_provider_,
              SendRouteBinaryMessageInternal(mojo::String(kRouteId), _, _))
      .WillOnce(Invoke([](
          const MediaRoute::Id& route_id, const std::vector<uint8>& data,
          const interfaces::MediaRouteProvider::SendRouteMessageCallback& cb) {
        EXPECT_EQ(
            0, memcmp(kBinaryMessage, &(data[0]), arraysize(kBinaryMessage)));
        cb.Run(true);
      }));

  SendMessageCallbackHandler handler;
  EXPECT_CALL(handler, Invoke(true));
  router()->SendRouteBinaryMessage(
      kRouteId, expected_binary_data.Pass(),
      base::Bind(&SendMessageCallbackHandler::Invoke,
                 base::Unretained(&handler)));
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, PresentationSessionMessagesObserver) {
  mojo::Array<interfaces::RouteMessagePtr> mojo_messages(2);
  mojo_messages[0] = interfaces::RouteMessage::New();
  mojo_messages[0]->type = interfaces::RouteMessage::Type::TYPE_TEXT;
  mojo_messages[0]->message = "text";
  mojo_messages[1] = interfaces::RouteMessage::New();
  mojo_messages[1]->type = interfaces::RouteMessage::Type::TYPE_BINARY;
  mojo_messages[1]->data.push_back(1);

  ScopedVector<content::PresentationSessionMessage> expected_messages;
  scoped_ptr<content::PresentationSessionMessage> message;
  message.reset(new content::PresentationSessionMessage(
      content::PresentationMessageType::TEXT));
  message->message = "text";
  expected_messages.push_back(message.Pass());

  message.reset(new content::PresentationSessionMessage(
      content::PresentationMessageType::ARRAY_BUFFER));
  message->data.reset(new std::vector<uint8_t>(1, 1));
  expected_messages.push_back(message.Pass());

  MediaRoute::Id expected_route_id("foo");
  interfaces::MediaRouteProvider::ListenForRouteMessagesCallback mojo_callback;
  EXPECT_CALL(mock_media_route_provider_,
              ListenForRouteMessages(Eq(expected_route_id), _))
      .WillOnce(SaveArg<1>(&mojo_callback));

  ListenForMessagesCallbackHandler handler(expected_messages.Pass());
  EXPECT_CALL(handler, InvokeObserver());
  // Creating PresentationSessionMessagesObserver will register itself to the
  // MediaRouter, which in turn will start listening for route messages.
  scoped_ptr<PresentationSessionMessagesObserver> observer(
      new PresentationSessionMessagesObserver(
          base::Bind(&ListenForMessagesCallbackHandler::Invoke,
                     base::Unretained(&handler)),
          expected_route_id, router()));
  ProcessEventLoop();

  // Simulate messages by invoking the saved mojo callback.
  // We expect one more ListenForRouteMessages call since |observer| was
  // still registered when the first set of messages arrived.
  mojo_callback.Run(mojo_messages.Pass());
  interfaces::MediaRouteProvider::ListenForRouteMessagesCallback
      mojo_callback_2;
  EXPECT_CALL(mock_media_route_provider_, ListenForRouteMessages(_, _))
      .WillOnce(SaveArg<1>(&mojo_callback_2));
  ProcessEventLoop();

  // Stop listening for messages. In particular, MediaRouterMojoImpl will not
  // call ListenForRouteMessages again when it sees there are no more observers.
  mojo::Array<interfaces::RouteMessagePtr> mojo_messages_2(1);
  mojo_messages_2[0] = interfaces::RouteMessage::New();
  mojo_messages_2[0]->type = interfaces::RouteMessage::Type::TYPE_TEXT;
  mojo_messages_2[0]->message = "foo";
  observer.reset();
  mojo_callback_2.Run(mojo_messages_2.Pass());
  ProcessEventLoop();
}

TEST_F(MediaRouterMojoImplTest, QueuedWhileAsleep) {
  EXPECT_CALL(mock_event_page_tracker_, IsEventPageSuspended(extension_id()))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_event_page_tracker_, WakeEventPage(extension_id(), _))
      .Times(2)
      .WillRepeatedly(Return(true));
  router()->CloseRoute(kRouteId);
  router()->CloseRoute(kRouteId2);
  ProcessEventLoop();
  EXPECT_CALL(mock_event_page_tracker_, IsEventPageSuspended(extension_id()))
      .Times(1)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_media_route_provider_, CloseRoute(mojo::String(kRouteId)));
  EXPECT_CALL(mock_media_route_provider_, CloseRoute(mojo::String(kRouteId2)));
  ConnectProviderManagerService();
  ProcessEventLoop();
}

// Temporarily disabled until the issues with extension system teardown
// are addressed.
// TODO(kmarshall): Re-enable this test. (http://crbug.com/490468)
TEST(MediaRouterMojoExtensionTest, DISABLED_DeferredBindingAndSuspension) {
  base::MessageLoop message_loop(mojo::common::MessagePumpMojo::Create());

  // Set up a mock ProcessManager instance.
  TestingProfile profile;
  extensions::ProcessManagerFactory::GetInstance()->SetTestingFactory(
      &profile, &TestProcessManager::Create);
  TestProcessManager* process_manager = static_cast<TestProcessManager*>(
      extensions::ProcessManager::Get(&profile));

  // Create MR and its proxy, so that it can be accessed through Mojo.
  MediaRouterMojoImpl media_router(process_manager);
  interfaces::MediaRouterPtr media_router_proxy;

  // Create a client object and its Mojo proxy.
  testing::StrictMock<MockMediaRouteProvider> mock_media_route_provider;
  interfaces::MediaRouteProviderPtr media_route_provider_proxy;

  // CloseRoute is called before *any* extension has connected.
  // It should be queued.
  media_router.CloseRoute(kRouteId);

  // Construct bindings so that |media_router| delegates calls to
  // |mojo_media_router|, which are then handled by
  // |mock_media_route_provider_service|.
  scoped_ptr<mojo::Binding<interfaces::MediaRouteProvider>> binding(
      new mojo::Binding<interfaces::MediaRouteProvider>(
          &mock_media_route_provider,
          mojo::GetProxy(&media_route_provider_proxy)));
  media_router.BindToMojoRequest(mojo::GetProxy(&media_router_proxy),
                                 kExtensionId);

  // |mojo_media_router| signals its readiness to the MR by registering
  // itself via RegisterMediaRouteProvider().
  // Now that the |media_router| and |mojo_media_router| are fully initialized,
  // the queued CloseRoute() call should be executed.
  RegisterMediaRouteProviderHandler provide_handler;
  EXPECT_CALL(provide_handler, Invoke(testing::Not("")));
  EXPECT_CALL(*process_manager, IsEventPageSuspended(kExtensionId))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_media_route_provider, CloseRoute(mojo::String(kRouteId)));
  media_router_proxy->RegisterMediaRouteProvider(
      media_route_provider_proxy.Pass(),
      base::Bind(&RegisterMediaRouteProviderHandler::Invoke,
                 base::Unretained(&provide_handler)));
  message_loop.RunUntilIdle();

  // Extension is suspended and re-awoken.
  binding.reset();
  media_router.BindToMojoRequest(mojo::GetProxy(&media_router_proxy),
                                 kExtensionId);
  EXPECT_CALL(*process_manager, IsEventPageSuspended(kExtensionId))
      .WillOnce(Return(true));
  EXPECT_CALL(*process_manager, WakeEventPage(kExtensionId, _))
      .WillOnce(testing::DoAll(media::RunCallback<1>(true), Return(true)));
  media_router.CloseRoute(kRouteId2);
  message_loop.RunUntilIdle();

  // RegisterMediaRouteProvider() is called.
  // The queued CloseRoute(kRouteId2) call should be executed.
  EXPECT_CALL(provide_handler, Invoke(testing::Not("")));
  EXPECT_CALL(*process_manager, IsEventPageSuspended(kExtensionId))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_media_route_provider, CloseRoute(mojo::String(kRouteId2)));
  binding.reset(new mojo::Binding<interfaces::MediaRouteProvider>(
      &mock_media_route_provider, mojo::GetProxy(&media_route_provider_proxy)));
  media_router_proxy->RegisterMediaRouteProvider(
      media_route_provider_proxy.Pass(),
      base::Bind(&RegisterMediaRouteProviderHandler::Invoke,
                 base::Unretained(&provide_handler)));
  message_loop.RunUntilIdle();
}

}  // namespace media_router
