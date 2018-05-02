// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/forwarding_audio_stream_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/unguessable_token.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/interfaces/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Test;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::StrictMock;
using ::testing::InSequence;

namespace content {

class MockBroker : public AudioStreamBroker {
 public:
  explicit MockBroker(RenderFrameHost* rfh)
      : AudioStreamBroker(rfh->GetProcess()->GetID(), rfh->GetRoutingID()),
        weak_factory_(this) {}

  ~MockBroker() override {}

  MOCK_METHOD1(CreateStream, void(audio::mojom::StreamFactory* factory));

  // Can be used to verify that |this| has been destructed.
  base::WeakPtr<MockBroker> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  int stream_id;
  base::UnguessableToken group_id;
  DeleterCallback deleter;

 private:
  base::WeakPtrFactory<MockBroker> weak_factory_;
};

class MockBrokerFactory : public AudioStreamBrokerFactory {
 public:
  MockBrokerFactory() {}
  ~MockBrokerFactory() final {
    EXPECT_TRUE(prepared_output_stream_brokers_.empty())
        << "Broker creation was expected but didn't happen";
  }

  void ExpectOutputStreamBrokerCreation(std::unique_ptr<MockBroker> broker) {
    prepared_output_stream_brokers_.push(std::move(broker));
  }

  std::unique_ptr<AudioStreamBroker> CreateAudioOutputStreamBroker(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      AudioStreamBroker::DeleterCallback deleter,
      media::mojom::AudioOutputStreamProviderClientPtr client) final {
    std::unique_ptr<MockBroker> prepared_broker =
        std::move(prepared_output_stream_brokers_.front());
    prepared_output_stream_brokers_.pop();
    CHECK_NE(nullptr, prepared_broker.get());
    EXPECT_EQ(render_process_id, prepared_broker->render_process_id());
    EXPECT_EQ(render_frame_id, prepared_broker->render_frame_id());
    prepared_broker->stream_id = stream_id;
    prepared_broker->group_id = group_id;
    prepared_broker->deleter = std::move(deleter);
    return std::move(prepared_broker);
  }

 private:
  base::queue<std::unique_ptr<MockBroker>> prepared_output_stream_brokers_;
};

class ForwardingAudioStreamFactoryTest : public RenderViewHostTestHarness {
 public:
  ForwardingAudioStreamFactoryTest()
      : connector_(service_manager::Connector::Create(&connector_request_)),
        broker_factory_(std::make_unique<MockBrokerFactory>()) {
    service_manager::Connector::TestApi connector_test_api(connector_.get());
    connector_test_api.OverrideBinderForTesting(
        service_manager::Identity(audio::mojom::kServiceName),
        audio::mojom::StreamFactory::Name_,
        base::BindRepeating(
            &ForwardingAudioStreamFactoryTest::SetPendingFactoryRequest,
            base::Unretained(this)));
  }

  ~ForwardingAudioStreamFactoryTest() override {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();
    other_rfh_ =
        RenderFrameHostTester::For(main_rfh())->AppendChild("sderfdgijho");
  }

  void SetPendingFactoryRequest(mojo::ScopedMessagePipeHandle factory_request) {
    pending_factory_request_ = std::move(factory_request);
  }

  base::WeakPtr<MockBroker> ExpectOutputBrokerConstruction(
      RenderFrameHost* rfh) {
    auto broker = std::make_unique<StrictMock<MockBroker>>(rfh);
    auto weak_broker = broker->GetWeakPtr();
    broker_factory_->ExpectOutputStreamBrokerCreation(std::move(broker));
    return weak_broker;
  }

  RenderFrameHost* other_rfh() { return other_rfh_; }

  static const char kDeviceId[];
  static const media::AudioParameters kParams;
  service_manager::mojom::ConnectorRequest connector_request_;
  std::unique_ptr<service_manager::Connector> connector_;
  mojo::ScopedMessagePipeHandle pending_factory_request_;
  RenderFrameHost* other_rfh_;
  std::unique_ptr<MockBrokerFactory> broker_factory_;
};

const char ForwardingAudioStreamFactoryTest::kDeviceId[] = "test device id";
const media::AudioParameters ForwardingAudioStreamFactoryTest::kParams =
    media::AudioParameters::UnavailableDeviceParams();

TEST_F(ForwardingAudioStreamFactoryTest,
       CreateOutputStream_CreatesOutputStream) {
  media::mojom::AudioOutputStreamProviderClientPtr client;
  base::WeakPtr<MockBroker> broker = ExpectOutputBrokerConstruction(main_rfh());

  ForwardingAudioStreamFactory factory(web_contents(), std::move(connector_),
                                       std::move(broker_factory_));

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  mojo::MakeRequest(&client);
  factory.CreateOutputStream(main_rfh(), kDeviceId, kParams, std::move(client));
}

TEST_F(ForwardingAudioStreamFactoryTest, DeleterCalled_DestroysOutputStream) {
  media::mojom::AudioOutputStreamProviderClientPtr client;
  base::WeakPtr<MockBroker> main_rfh_broker =
      ExpectOutputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_broker =
      ExpectOutputBrokerConstruction(other_rfh());

  ForwardingAudioStreamFactory factory(web_contents(), std::move(connector_),
                                       std::move(broker_factory_));

  {
    EXPECT_CALL(*main_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.CreateOutputStream(main_rfh(), kDeviceId, kParams,
                               std::move(client));
    testing::Mock::VerifyAndClear(&*main_rfh_broker);
  }
  {
    EXPECT_CALL(*other_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.CreateOutputStream(other_rfh(), kDeviceId, kParams,
                               std::move(client));
    testing::Mock::VerifyAndClear(&*other_rfh_broker);
  }

  std::move(other_rfh_broker->deleter).Run(&*other_rfh_broker);
  EXPECT_FALSE(other_rfh_broker)
      << "Broker should be destructed when deleter is called.";
  EXPECT_TRUE(main_rfh_broker);
}

TEST_F(ForwardingAudioStreamFactoryTest,
       DestroyFrame_DestroysRelatedOutputStream) {
  media::mojom::AudioOutputStreamProviderClientPtr client;
  base::WeakPtr<MockBroker> main_rfh_broker =
      ExpectOutputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_broker =
      ExpectOutputBrokerConstruction(other_rfh());

  ForwardingAudioStreamFactory factory(web_contents(), std::move(connector_),
                                       std::move(broker_factory_));

  {
    EXPECT_CALL(*main_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.CreateOutputStream(main_rfh(), kDeviceId, kParams,
                               std::move(client));
    testing::Mock::VerifyAndClear(&*main_rfh_broker);
  }
  {
    EXPECT_CALL(*other_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.CreateOutputStream(other_rfh(), kDeviceId, kParams,
                               std::move(client));
    testing::Mock::VerifyAndClear(&*other_rfh_broker);
  }

  factory.FrameDeleted(other_rfh());
  EXPECT_FALSE(other_rfh_broker)
      << "Broker should be destructed when owning frame is destructed.";
  EXPECT_TRUE(main_rfh_broker);
}

TEST_F(ForwardingAudioStreamFactoryTest,
       DestroyWebContents_DestroysOutputStream) {
  media::mojom::AudioOutputStreamProviderClientPtr client;
  base::WeakPtr<MockBroker> broker = ExpectOutputBrokerConstruction(main_rfh());

  ForwardingAudioStreamFactory factory(web_contents(), std::move(connector_),
                                       std::move(broker_factory_));

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  mojo::MakeRequest(&client);
  factory.CreateOutputStream(main_rfh(), kDeviceId, kParams, std::move(client));

  DeleteContents();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(broker)
      << "Broker should be destructed when owning WebContents is destructed.";
}

TEST_F(ForwardingAudioStreamFactoryTest, DestroyRemoteFactory_CleansUpStreams) {
  media::mojom::AudioOutputStreamProviderClientPtr client;
  base::WeakPtr<MockBroker> broker = ExpectOutputBrokerConstruction(main_rfh());

  ForwardingAudioStreamFactory factory(web_contents(), std::move(connector_),
                                       std::move(broker_factory_));

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  mojo::MakeRequest(&client);
  factory.CreateOutputStream(main_rfh(), kDeviceId, kParams, std::move(client));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(broker);
  pending_factory_request_.reset();  // Triggers connection error.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(broker)
      << "Broker should be destructed when owning WebContents is destructed.";
}

TEST_F(ForwardingAudioStreamFactoryTest, LastStreamDeleted_ClearsFactoryPtr) {
  media::mojom::AudioOutputStreamProviderClientPtr client;
  base::WeakPtr<MockBroker> main_rfh_broker =
      ExpectOutputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_broker =
      ExpectOutputBrokerConstruction(other_rfh());

  ForwardingAudioStreamFactory factory(web_contents(), std::move(connector_),
                                       std::move(broker_factory_));

  {
    EXPECT_CALL(*main_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.CreateOutputStream(main_rfh(), kDeviceId, kParams,
                               std::move(client));
    testing::Mock::VerifyAndClear(&*main_rfh_broker);
  }
  {
    EXPECT_CALL(*other_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.CreateOutputStream(other_rfh(), kDeviceId, kParams,
                               std::move(client));
    testing::Mock::VerifyAndClear(&*other_rfh_broker);
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(pending_factory_request_->QuerySignalsState().peer_closed());
  std::move(other_rfh_broker->deleter).Run(&*other_rfh_broker);
  base::RunLoop().RunUntilIdle();
  // Connection should still be open, since there's still a stream left.
  EXPECT_FALSE(pending_factory_request_->QuerySignalsState().peer_closed());
  std::move(main_rfh_broker->deleter).Run(&*main_rfh_broker);
  base::RunLoop().RunUntilIdle();
  // Now there are no streams left, connection should be broken.
  EXPECT_TRUE(pending_factory_request_->QuerySignalsState().peer_closed());
}

}  // namespace content
