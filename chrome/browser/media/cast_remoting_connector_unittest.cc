// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_remoting_connector.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/route_message.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/mojo/interfaces/remoting.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

using media::mojom::RemoterPtr;
using media::mojom::RemoterRequest;
using media::mojom::RemotingSinkCapabilities;
using media::mojom::RemotingSourcePtr;
using media::mojom::RemotingSourceRequest;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;

using media_router::MediaRoutesObserver;
using media_router::MediaRoute;
using media_router::MediaSource;
using media_router::RouteMessage;
using media_router::RouteMessageObserver;

using ::testing::_;
using ::testing::AtLeast;

namespace {

constexpr char kRemotingMediaSource[] =
    "urn:x-org.chromium.media:source:tab_content_remoting:123";
constexpr char kRemotingMediaSink[] = "wiggles";
constexpr char kRemotingMediaRoute[] =
    "urn:x-org.chromium:media:route:garbly_gook_ssi7m4oa8oma7rasd/cast-wiggles";

constexpr char kTabMirroringMediaSource[] =
    "urn:x-org.chromium.media:source:tab:123";
constexpr char kTabMirroringMediaRoute[] =
    "urn:x-org.chromium:media:route:bloopity_blop_ohun48i56nh9oid/cast-wiggles";

constexpr RemotingSinkCapabilities kAllCapabilities =
    RemotingSinkCapabilities::CONTENT_DECRYPTION_AND_RENDERING;

// Implements basic functionality of a subset of the MediaRouter for use by the
// unit tests in this module. Note that MockMediaRouter will complain at runtime
// if any methods were called that should not have been called.
class FakeMediaRouter : public media_router::MockMediaRouter {
 public:
  FakeMediaRouter()
      : routes_observer_(nullptr), message_observer_(nullptr),
        weak_factory_(this) {}
  ~FakeMediaRouter() final {}

  //
  // These methods are called by test code to create/destroy a media route and
  // pass messages in both directions.
  //

  void OnRemotingRouteExists(bool exists) {
    routes_.clear();

    // Always add a non-remoting route to make sure CastRemotingConnector
    // ignores non-remoting routes.
    routes_.push_back(MediaRoute(
        kTabMirroringMediaRoute, MediaSource(kTabMirroringMediaSource),
        kRemotingMediaSink, "Cast Tab Mirroring", false, "", false));

    if (exists) {
      routes_.push_back(MediaRoute(
          kRemotingMediaRoute, MediaSource(kRemotingMediaSource),
          kRemotingMediaSink, "Cast Media Remoting", false, "", false));
    } else {
      // Cancel delivery of all messages in both directions.
      inbound_messages_.clear();
      for (const auto& entry : outbound_messages_) {
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                base::Bind(entry.second, false));
      }
      outbound_messages_.clear();
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&FakeMediaRouter::DoUpdateRoutes,
                   weak_factory_.GetWeakPtr()));
  }

  void OnMessageFromProvider(const std::string& message) {
    inbound_messages_.push_back(RouteMessage());
    inbound_messages_.back().type = RouteMessage::TEXT;
    inbound_messages_.back().text = message;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&FakeMediaRouter::DoDeliverInboundMessages,
                   weak_factory_.GetWeakPtr()));
  }

  void OnBinaryMessageFromProvider(const std::vector<uint8_t>& message) {
    inbound_messages_.push_back(RouteMessage());
    inbound_messages_.back().type = RouteMessage::BINARY;
    inbound_messages_.back().binary = std::vector<uint8_t>(message);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&FakeMediaRouter::DoDeliverInboundMessages,
                   weak_factory_.GetWeakPtr()));
  }

  void TakeMessagesSentToProvider(RouteMessage::Type type,
                                  std::vector<RouteMessage>* messages) {
    decltype(outbound_messages_) untaken_messages;
    for (const auto& entry : outbound_messages_) {
      if (entry.first.type == type) {
        messages->push_back(entry.first);
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                base::Bind(entry.second, true));
      } else {
        untaken_messages.push_back(entry);
      }
    }
    outbound_messages_.swap(untaken_messages);
  }

 protected:
  void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) final {
    CHECK(!routes_observer_);
    routes_observer_ = observer;
    CHECK(routes_observer_);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&FakeMediaRouter::DoUpdateRoutes,
                   weak_factory_.GetWeakPtr()));
  }

  void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) final {
    CHECK_EQ(routes_observer_, observer);
    routes_observer_ = nullptr;
  }

  void RegisterRouteMessageObserver(RouteMessageObserver* observer) final {
    CHECK(!message_observer_);
    message_observer_ = observer;
    CHECK(message_observer_);
  }

  void UnregisterRouteMessageObserver(RouteMessageObserver* observer) final {
    CHECK_EQ(message_observer_, observer);
    message_observer_ = nullptr;
  }

  void SendRouteMessage(const MediaRoute::Id& route_id,
                        const std::string& text,
                        const SendRouteMessageCallback& callback) final {
    EXPECT_EQ(message_observer_->route_id(), route_id);
    ASSERT_FALSE(callback.is_null());
    RouteMessage message;
    message.type = RouteMessage::TEXT;
    message.text = text;
    outbound_messages_.push_back(std::make_pair(message, callback));
  }

  void SendRouteBinaryMessage(
      const MediaRoute::Id& route_id,
      std::unique_ptr<std::vector<uint8_t>> data,
      const SendRouteMessageCallback& callback) final {
    EXPECT_EQ(message_observer_->route_id(), route_id);
    ASSERT_TRUE(!!data);
    ASSERT_FALSE(callback.is_null());
    RouteMessage message;
    message.type = RouteMessage::BINARY;
    message.binary = std::move(*data);
    outbound_messages_.push_back(std::make_pair(message, callback));
  }

 private:
  // Asynchronous callback to notify the MediaRoutesObserver of a change in
  // routes.
  void DoUpdateRoutes() {
    if (routes_observer_)
      routes_observer_->OnRoutesUpdated(routes_, std::vector<MediaRoute::Id>());
  }

  // Asynchronous callback to deliver messages to the RouteMessageObserver.
  void DoDeliverInboundMessages() {
    if (message_observer_)
      message_observer_->OnMessagesReceived(inbound_messages_);
    inbound_messages_.clear();
  }

  MediaRoutesObserver* routes_observer_;
  RouteMessageObserver* message_observer_;

  std::vector<MediaRoute> routes_;
  // Messages from Cast Provider to the connector.
  std::vector<RouteMessage> inbound_messages_;
  // Messages from the connector to the Cast Provider.
  using OutboundMessageAndCallback =
      std::pair<RouteMessage, SendRouteMessageCallback>;
  std::vector<OutboundMessageAndCallback> outbound_messages_;

  base::WeakPtrFactory<FakeMediaRouter> weak_factory_;
};

class MockRemotingSource : public media::mojom::RemotingSource {
 public:
  MockRemotingSource() : binding_(this) {}
  ~MockRemotingSource() final {}

  void Bind(RemotingSourceRequest request) {
    binding_.Bind(std::move(request));
  }

  MOCK_METHOD1(OnSinkAvailable, void(RemotingSinkCapabilities));
  MOCK_METHOD0(OnSinkGone, void());
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD1(OnStartFailed, void(RemotingStartFailReason));
  MOCK_METHOD1(OnMessageFromSink, void(const std::vector<uint8_t>&));
  MOCK_METHOD1(OnStopped, void(RemotingStopReason));

 private:
  mojo::Binding<media::mojom::RemotingSource> binding_;
};

}  // namespace

class CastRemotingConnectorTest : public ::testing::Test {
 public:
  CastRemotingConnectorTest()
      : connector_(&media_router_, kRemotingMediaSource) {
    // HACK: Override feature flags for testing.
    const_cast<RemotingSinkCapabilities&>(connector_.enabled_features_) =
        kAllCapabilities;
  }

  void TearDown() final {
    // Allow any pending Mojo operations to complete before destruction. For
    // example, when one end of a Mojo message pipe is closed, a task is posted
    // to later destroy objects that were owned by the message pipe.
    RunUntilIdle();
  }

 protected:
  RemoterPtr CreateRemoter(MockRemotingSource* source) {
    RemotingSourcePtr source_ptr;
    source->Bind(mojo::MakeRequest(&source_ptr));
    RemoterPtr remoter_ptr;
    connector_.CreateBridge(std::move(source_ptr),
                            mojo::MakeRequest(&remoter_ptr));
    return remoter_ptr;
  }

  void ProviderDiscoversSink() {
    media_router_.OnRemotingRouteExists(true);
  }

  void ProviderLosesSink() {
    media_router_.OnRemotingRouteExists(false);
  }

  void ConnectorSentMessageToProvider(const std::string& expected_message) {
    std::vector<RouteMessage> messages;
    media_router_.TakeMessagesSentToProvider(RouteMessage::TEXT, &messages);
    bool did_see_expected_message = false;
    for (const RouteMessage& message : messages) {
      if (message.text && expected_message == *message.text) {
        did_see_expected_message = true;
      } else {
        ADD_FAILURE()
            << "Unexpected message: " << message.ToHumanReadableString();
      }
    }
    EXPECT_TRUE(did_see_expected_message);
  }

  void ConnectorSentMessageToSink(
      const std::vector<uint8_t>& expected_message) {
    std::vector<RouteMessage> messages;
    media_router_.TakeMessagesSentToProvider(RouteMessage::BINARY, &messages);
    bool did_see_expected_message = false;
    for (const RouteMessage& message : messages) {
      if (message.binary && expected_message == *message.binary) {
        did_see_expected_message = true;
      } else {
        ADD_FAILURE()
            << "Unexpected message: " << message.ToHumanReadableString();
      }
    }
    EXPECT_TRUE(did_see_expected_message);
  }

  void ConnectorSentNoMessagesToProvider() {
    std::vector<RouteMessage> messages;
    media_router_.TakeMessagesSentToProvider(RouteMessage::TEXT, &messages);
    EXPECT_TRUE(messages.empty());
  }

  void ConnectorSentNoMessagesToSink() {
    std::vector<RouteMessage> messages;
    media_router_.TakeMessagesSentToProvider(RouteMessage::BINARY, &messages);
    EXPECT_TRUE(messages.empty());
  }

  void ProviderPassesMessageFromSink(
      const std::vector<uint8_t>& message) {
    media_router_.OnBinaryMessageFromProvider(message);
  }

  void ProviderSaysToRemotingConnector(const std::string& message) {
    media_router_.OnMessageFromProvider(message);
  }

  void MediaRouterTerminatesRoute() {
    media_router_.OnRemotingRouteExists(false);
  }

  static void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
  }

 private:
  content::TestBrowserThreadBundle browser_thread_bundle_;
  FakeMediaRouter media_router_;
  CastRemotingConnector connector_;
};

TEST_F(CastRemotingConnectorTest, NeverNotifiesThatSinkIsAvailable) {
  MockRemotingSource source;
  RemoterPtr remoter = CreateRemoter(&source);

  EXPECT_CALL(source, OnSinkAvailable(_)).Times(0);
  EXPECT_CALL(source, OnSinkGone()).Times(AtLeast(0));
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, NotifiesWhenSinkIsAvailableAndThenGone) {
  MockRemotingSource source;
  RemoterPtr remoter = CreateRemoter(&source);

  EXPECT_CALL(source, OnSinkAvailable(kAllCapabilities)).Times(1);
  ProviderDiscoversSink();
  RunUntilIdle();

  EXPECT_CALL(source, OnSinkGone()).Times(AtLeast(1));
  ProviderLosesSink();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest,
       NotifiesMultipleSourcesWhenSinkIsAvailableAndThenGone) {
  MockRemotingSource source1;
  RemoterPtr remoter1 = CreateRemoter(&source1);
  MockRemotingSource source2;
  RemoterPtr remoter2 = CreateRemoter(&source2);

  EXPECT_CALL(source1, OnSinkAvailable(kAllCapabilities)).Times(1);
  EXPECT_CALL(source2, OnSinkAvailable(kAllCapabilities)).Times(1);
  ProviderDiscoversSink();
  RunUntilIdle();

  EXPECT_CALL(source1, OnSinkGone()).Times(AtLeast(1));
  EXPECT_CALL(source2, OnSinkGone()).Times(AtLeast(1));
  ProviderLosesSink();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, HandlesTeardownOfRemotingSourceFirst) {
  std::unique_ptr<MockRemotingSource> source(new MockRemotingSource);
  RemoterPtr remoter = CreateRemoter(source.get());

  EXPECT_CALL(*source, OnSinkAvailable(kAllCapabilities)).Times(1);
  ProviderDiscoversSink();
  RunUntilIdle();

  source.reset();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, HandlesTeardownOfRemoterFirst) {
  MockRemotingSource source;
  RemoterPtr remoter = CreateRemoter(&source);

  EXPECT_CALL(source, OnSinkAvailable(kAllCapabilities)).Times(1);
  ProviderDiscoversSink();
  RunUntilIdle();

  remoter.reset();
  RunUntilIdle();
}

namespace {

// The possible ways a remoting session may be terminated in the "full
// run-through" tests.
enum HowItEnds {
  SOURCE_TERMINATES,  // The render process decides to end remoting.
  MOJO_PIPE_CLOSES,   // A Mojo message pipe closes unexpectedly.
  ROUTE_TERMINATES,   // The Media Router UI was used to terminate the route.
  EXTERNAL_FAILURE,   // The sink is cut-off, perhaps due to a network outage.
};

}  // namespace

class CastRemotingConnectorFullSessionTest
    : public CastRemotingConnectorTest,
      public ::testing::WithParamInterface<HowItEnds> {
 public:
  HowItEnds how_it_ends() const { return GetParam(); }
};

// Performs a full run-through of starting and stopping remoting, with
// communications between source and sink established at the correct times, and
// tests that end-to-end behavior is correct depending on what caused the
// remoting session to end.
TEST_P(CastRemotingConnectorFullSessionTest, GoesThroughAllTheMotions) {
  std::unique_ptr<MockRemotingSource> source(new MockRemotingSource());
  RemoterPtr remoter = CreateRemoter(source.get());
  std::unique_ptr<MockRemotingSource> other_source(new MockRemotingSource());
  RemoterPtr other_remoter = CreateRemoter(other_source.get());

  // Throughout this test |other_source| should not participate in the
  // remoting session, and so these method calls should never occur:
  EXPECT_CALL(*other_source, OnStarted()).Times(0);
  EXPECT_CALL(*other_source, OnStopped(_)).Times(0);
  EXPECT_CALL(*other_source, OnMessageFromSink(_)).Times(0);

  // Both sinks should be notified when the Cast Provider tells the connector
  // a remoting sink is available.
  EXPECT_CALL(*source, OnSinkAvailable(kAllCapabilities)).Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*other_source, OnSinkAvailable(kAllCapabilities)).Times(1)
      .RetiresOnSaturation();
  ProviderDiscoversSink();
  RunUntilIdle();

  // When |source| starts a remoting session, |other_source| is notified the
  // sink is gone, the Cast Provider is notified that remoting has started,
  // and |source| is notified that its request was successful.
  EXPECT_CALL(*source, OnStarted()).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*other_source, OnSinkGone()).Times(1).RetiresOnSaturation();
  remoter->Start();
  RunUntilIdle();
  ConnectorSentMessageToProvider("START_CAST_REMOTING:session=1");

  // The |source| should now be able to send binary messages to the sink.
  // |other_source| should not!
  const std::vector<uint8_t> message_to_sink = { 3, 1, 4, 1, 5, 9 };
  remoter->SendMessageToSink(message_to_sink);
  const std::vector<uint8_t> ignored_message_to_sink = { 1, 2, 3 };
  other_remoter->SendMessageToSink(ignored_message_to_sink);
  RunUntilIdle();
  ConnectorSentMessageToSink(message_to_sink);

  // The sink should also be able to send binary messages to the |source|.
  const std::vector<uint8_t> message_to_source = { 2, 7, 1, 8, 2, 8 };
  EXPECT_CALL(*source, OnMessageFromSink(message_to_source)).Times(1)
      .RetiresOnSaturation();
  ProviderPassesMessageFromSink(message_to_source);
  RunUntilIdle();

  // The |other_source| should not be allowed to start a remoting session.
  EXPECT_CALL(*other_source,
              OnStartFailed(RemotingStartFailReason::CANNOT_START_MULTIPLE))
      .Times(1).RetiresOnSaturation();
  other_remoter->Start();
  RunUntilIdle();
  ConnectorSentNoMessagesToProvider();

  // What happens from here depends on how this remoting session will end...
  switch (how_it_ends()) {
    case SOURCE_TERMINATES: {
      // When the |source| stops the remoting session, the Cast Provider is
      // notified the session has stopped, and the |source| receives both an
      // OnStopped() and an OnSinkGone() notification.
      const RemotingStopReason reason = RemotingStopReason::LOCAL_PLAYBACK;
      EXPECT_CALL(*source, OnSinkGone()).Times(1).RetiresOnSaturation();
      EXPECT_CALL(*source, OnStopped(reason)).Times(1).RetiresOnSaturation();
      remoter->Stop(reason);
      RunUntilIdle();
      ConnectorSentMessageToProvider("STOP_CAST_REMOTING:session=1");

      // Since remoting is stopped, any further messaging in either direction
      // must be dropped.
      const std::vector<uint8_t> message_to_sink = { 1, 6, 1, 8, 0, 3 };
      const std::vector<uint8_t> message_to_source = { 6, 2, 8, 3, 1, 8 };
      EXPECT_CALL(*source, OnMessageFromSink(_)).Times(0);
      remoter->SendMessageToSink(message_to_sink);
      ProviderPassesMessageFromSink(message_to_source);
      RunUntilIdle();
      ConnectorSentNoMessagesToSink();

      // When the sink is ready, the Cast Provider sends a notification to the
      // connector. The connector will notify both sources that a sink is once
      // again available.
      EXPECT_CALL(*source, OnSinkAvailable(kAllCapabilities)).Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*other_source, OnSinkAvailable(kAllCapabilities)).Times(1)
          .RetiresOnSaturation();
      ProviderSaysToRemotingConnector("STOPPED_CAST_REMOTING:session=1");
      RunUntilIdle();

      // When the sink is no longer available, the Cast Provider notifies the
      // connector, and both sources are then notified the sink is gone.
      EXPECT_CALL(*source, OnSinkGone()).Times(AtLeast(1));
      EXPECT_CALL(*other_source, OnSinkGone()).Times(AtLeast(1));
      ProviderLosesSink();
      RunUntilIdle();

      break;
    }

    case MOJO_PIPE_CLOSES:
      // When the Mojo pipes for |other_source| close, this should not affect
      // the current remoting session.
      other_source.reset();
      other_remoter.reset();
      RunUntilIdle();
      ConnectorSentNoMessagesToProvider();

      // Now, when the Mojo pipes for |source| close, the Cast Provider will be
      // notified that the session has stopped.
      source.reset();
      remoter.reset();
      RunUntilIdle();
      ConnectorSentMessageToProvider("STOP_CAST_REMOTING:session=1");

      // The Cast Provider will detect when the sink is ready for the next
      // remoting session, and then notify the connector. However, there are no
      // sources to propagate this notification to.
      ProviderSaysToRemotingConnector("STOPPED_CAST_REMOTING:session=1");
      RunUntilIdle();

      break;

    case ROUTE_TERMINATES:
      // When the Media Router terminates the route (e.g., because a user
      // terminated the route from the UI), the source and sink are immediately
      // cut off from one another.
      EXPECT_CALL(*source, OnSinkGone()).Times(AtLeast(1));
      EXPECT_CALL(*source, OnStopped(RemotingStopReason::ROUTE_TERMINATED))
          .Times(1).RetiresOnSaturation();
      EXPECT_CALL(*other_source, OnSinkGone()).Times(AtLeast(0));
      MediaRouterTerminatesRoute();
      RunUntilIdle();
      ConnectorSentNoMessagesToProvider();

      // Furthermore, the connector and Cast Provider are also cut off from one
      // another and should not be able to exchange messages anymore. Therefore,
      // the connector will never try to notify the sources that the sink is
      // available again.
      EXPECT_CALL(*source, OnSinkAvailable(_)).Times(0);
      EXPECT_CALL(*other_source, OnSinkAvailable(_)).Times(0);
      ProviderSaysToRemotingConnector("STOPPED_CAST_REMOTING:session=1");
      RunUntilIdle();

      break;

    case EXTERNAL_FAILURE: {
      // When the Cast Provider is cut-off from the sink, it sends a fail
      // notification to the connector. The connector, in turn, force-stops the
      // remoting session and notifies the |source|.
      EXPECT_CALL(*source, OnSinkGone()).Times(1).RetiresOnSaturation();
      EXPECT_CALL(*source, OnStopped(RemotingStopReason::UNEXPECTED_FAILURE))
          .Times(1).RetiresOnSaturation();
      ProviderSaysToRemotingConnector("FAILED_CAST_REMOTING:session=1");
      RunUntilIdle();

      // Since remoting is stopped, any further messaging in either direction
      // must be dropped.
      const std::vector<uint8_t> message_to_sink = { 1, 6, 1, 8, 0, 3 };
      const std::vector<uint8_t> message_to_source = { 6, 2, 8, 3, 1, 8 };
      EXPECT_CALL(*source, OnMessageFromSink(_)).Times(0);
      remoter->SendMessageToSink(message_to_sink);
      ProviderPassesMessageFromSink(message_to_source);
      RunUntilIdle();
      ConnectorSentNoMessagesToSink();

      // Later, if whatever caused the external failure has resolved, the Cast
      // Provider will notify the connector that the sink is available one
      // again.
      EXPECT_CALL(*source, OnSinkAvailable(kAllCapabilities)).Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*other_source, OnSinkAvailable(kAllCapabilities)).Times(1)
          .RetiresOnSaturation();
      ProviderSaysToRemotingConnector("STOPPED_CAST_REMOTING:session=1");
      RunUntilIdle();

      // When the sink is no longer available, the Cast Provider notifies the
      // connector, and both sources are then notified the sink is gone.
      EXPECT_CALL(*source, OnSinkGone()).Times(AtLeast(1));
      EXPECT_CALL(*other_source, OnSinkGone()).Times(AtLeast(1));
      ProviderLosesSink();
      RunUntilIdle();

      break;
    }
  }
}

INSTANTIATE_TEST_CASE_P(, CastRemotingConnectorFullSessionTest,
                        ::testing::Values(SOURCE_TERMINATES,
                                          MOJO_PIPE_CLOSES,
                                          ROUTE_TERMINATES,
                                          EXTERNAL_FAILURE));
