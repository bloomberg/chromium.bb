// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>

#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/browser_connection_handler.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/test_common.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;

namespace blimp {
namespace {

// Compares two blimp messages and ignores message_id field.
MATCHER_P(EqualsMessageIgnoringId, message, "") {
  BlimpMessage expected_message = message;
  expected_message.clear_message_id();
  BlimpMessage actual_message = arg;
  actual_message.clear_message_id();

  std::string expected_serialized;
  std::string actual_serialized;
  expected_message.SerializeToString(&expected_serialized);
  actual_message.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

class FakeFeature {
 public:
  FakeFeature(BlimpMessage::Type type,
              BrowserConnectionHandler* connection_handler) {
    outgoing_message_processor_ =
        connection_handler->RegisterFeature(type, &incoming_message_processor_);
  }

  ~FakeFeature() {}

  BlimpMessageProcessor* outgoing_message_processor() {
    return outgoing_message_processor_.get();
  }

  MockBlimpMessageProcessor* incoming_message_processor() {
    return &incoming_message_processor_;
  }

 private:
  testing::StrictMock<MockBlimpMessageProcessor> incoming_message_processor_;
  scoped_ptr<BlimpMessageProcessor> outgoing_message_processor_;
};

class FakeBlimpConnection : public BlimpConnection,
                            public BlimpMessageProcessor {
 public:
  FakeBlimpConnection() {}
  ~FakeBlimpConnection() override {}

  void set_other_end(FakeBlimpConnection* other_end) { other_end_ = other_end; }

  ConnectionErrorObserver* error_observer() { return error_observer_; }

  // BlimpConnection implementation.
  void AddConnectionErrorObserver(ConnectionErrorObserver* observer) override {
    error_observer_ = observer;
  }

  void SetIncomingMessageProcessor(BlimpMessageProcessor* processor) override {
    incoming_message_processor_ = processor;
  }

  BlimpMessageProcessor* GetOutgoingMessageProcessor() override { return this; }

 private:
  void ForwardMessage(scoped_ptr<BlimpMessage> message) {
    other_end_->incoming_message_processor_->ProcessMessage(
        std::move(message), net::CompletionCallback());
  }

  // BlimpMessageProcessor implementation.
  void ProcessMessage(scoped_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&FakeBlimpConnection::ForwardMessage,
                              base::Unretained(this), base::Passed(&message)));

    callback.Run(net::OK);
  }

  FakeBlimpConnection* other_end_ = nullptr;
  ConnectionErrorObserver* error_observer_ = nullptr;
  BlimpMessageProcessor* incoming_message_processor_ = nullptr;
};

scoped_ptr<BlimpMessage> CreateInputMessage(int tab_id) {
  scoped_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::INPUT);
  output->set_target_tab_id(tab_id);
  return output;
}

scoped_ptr<BlimpMessage> CreateControlMessage(int tab_id) {
  scoped_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::TAB_CONTROL);
  output->set_target_tab_id(tab_id);
  return output;
}

class BrowserConnectionHandlerTest : public testing::Test {
 public:
  BrowserConnectionHandlerTest()
      : client_connection_handler_(new BrowserConnectionHandler),
        engine_connection_handler_(new BrowserConnectionHandler) {
    SetupConnections();

    client_input_feature_.reset(
        new FakeFeature(BlimpMessage::INPUT, client_connection_handler_.get()));
    engine_input_feature_.reset(
        new FakeFeature(BlimpMessage::INPUT, engine_connection_handler_.get()));
    client_control_feature_.reset(new FakeFeature(
        BlimpMessage::TAB_CONTROL, client_connection_handler_.get()));
    engine_control_feature_.reset(new FakeFeature(
        BlimpMessage::TAB_CONTROL, engine_connection_handler_.get()));
  }

  ~BrowserConnectionHandlerTest() override {}
  void TearDown() override { base::RunLoop().RunUntilIdle(); }

 protected:
  void SetupConnections() {
    client_connection_ = new FakeBlimpConnection();
    engine_connection_ = new FakeBlimpConnection();
    client_connection_->set_other_end(engine_connection_);
    engine_connection_->set_other_end(client_connection_);
    client_connection_handler_->HandleConnection(
        make_scoped_ptr(client_connection_));
    engine_connection_handler_->HandleConnection(
        make_scoped_ptr(engine_connection_));
  }

  base::MessageLoop message_loop_;

  FakeBlimpConnection* client_connection_;
  FakeBlimpConnection* engine_connection_;

  scoped_ptr<BrowserConnectionHandler> client_connection_handler_;
  scoped_ptr<BrowserConnectionHandler> engine_connection_handler_;

  scoped_ptr<FakeFeature> client_input_feature_;
  scoped_ptr<FakeFeature> engine_input_feature_;
  scoped_ptr<FakeFeature> client_control_feature_;
  scoped_ptr<FakeFeature> engine_control_feature_;
};

TEST_F(BrowserConnectionHandlerTest, ExchangeMessages) {
  scoped_ptr<BlimpMessage> client_input_message = CreateInputMessage(1);
  scoped_ptr<BlimpMessage> client_control_message = CreateControlMessage(1);
  scoped_ptr<BlimpMessage> engine_control_message = CreateControlMessage(2);

  EXPECT_CALL(
      *(engine_input_feature_->incoming_message_processor()),
      MockableProcessMessage(EqualsMessageIgnoringId(*client_input_message), _))
      .RetiresOnSaturation();
  EXPECT_CALL(*(engine_control_feature_->incoming_message_processor()),
              MockableProcessMessage(
                  EqualsMessageIgnoringId(*client_control_message), _))
      .RetiresOnSaturation();
  EXPECT_CALL(*(client_control_feature_->incoming_message_processor()),
              MockableProcessMessage(
                  EqualsMessageIgnoringId(*engine_control_message), _))
      .RetiresOnSaturation();

  client_input_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(client_input_message), net::CompletionCallback());
  client_control_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(client_control_message), net::CompletionCallback());
  engine_control_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(engine_control_message), net::CompletionCallback());
}

TEST_F(BrowserConnectionHandlerTest, ConnectionError) {
  // Engine will not get message after connection error.
  client_connection_->error_observer()->OnConnectionError(net::ERR_FAILED);
  scoped_ptr<BlimpMessage> client_input_message = CreateInputMessage(1);
  client_input_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(client_input_message), net::CompletionCallback());
}

TEST_F(BrowserConnectionHandlerTest, ReconnectionAfterError) {
  scoped_ptr<BlimpMessage> client_input_message = CreateInputMessage(1);
  EXPECT_CALL(
      *(engine_input_feature_->incoming_message_processor()),
      MockableProcessMessage(EqualsMessageIgnoringId(*client_input_message), _))
      .RetiresOnSaturation();

  // Simulate a connection failure.
  client_connection_->error_observer()->OnConnectionError(net::ERR_FAILED);
  engine_connection_->error_observer()->OnConnectionError(net::ERR_FAILED);

  // Message will be queued to be transmitted when the connection is
  // re-established.
  client_input_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(client_input_message), net::CompletionCallback());

  // Simulates reconnection.
  SetupConnections();
}

}  // namespace
}  // namespace blimp
