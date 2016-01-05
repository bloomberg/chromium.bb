// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_transport.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/engine_authentication_handler.h"
#include "blimp/net/test_common.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::SaveArg;

namespace blimp {

class EngineAuthenticationHandlerTest : public testing::Test {
 public:
  EngineAuthenticationHandlerTest()
      : runner_(new base::TestMockTimeTaskRunner),
        runner_handle_(runner_),
        auth_handler_(new EngineAuthenticationHandler(&connection_handler_)),
        connection_(new testing::StrictMock<MockBlimpConnection>()) {}

  ~EngineAuthenticationHandlerTest() override {}

 protected:
  void ExpectOnConnection() {
    EXPECT_CALL(*connection_, AddConnectionErrorObserver(_))
        .WillOnce(SaveArg<0>(&error_observer_));
    EXPECT_CALL(*connection_, SetIncomingMessageProcessor(_))
        .WillOnce(SaveArg<0>(&incoming_message_processor_));
  }

  scoped_refptr<base::TestMockTimeTaskRunner> runner_;
  base::ThreadTaskRunnerHandle runner_handle_;
  testing::StrictMock<MockConnectionHandler> connection_handler_;
  scoped_ptr<EngineAuthenticationHandler> auth_handler_;
  scoped_ptr<testing::StrictMock<MockBlimpConnection>> connection_;
  ConnectionErrorObserver* error_observer_ = nullptr;
  BlimpMessageProcessor* incoming_message_processor_ = nullptr;
};

TEST_F(EngineAuthenticationHandlerTest, AuthenticationSucceeds) {
  ExpectOnConnection();
  EXPECT_CALL(*connection_, RemoveConnectionErrorObserver(_));
  EXPECT_CALL(connection_handler_, HandleConnectionPtr(Eq(connection_.get())));
  auth_handler_->HandleConnection(std::move(connection_));
  EXPECT_NE(nullptr, error_observer_);
  EXPECT_NE(nullptr, incoming_message_processor_);

  scoped_ptr<BlimpMessage> blimp_message = CreateStartConnectionMessage("", 0);
  net::TestCompletionCallback process_message_cb;
  incoming_message_processor_->ProcessMessage(std::move(blimp_message),
                                              process_message_cb.callback());
  EXPECT_EQ(net::OK, process_message_cb.WaitForResult());
}

TEST_F(EngineAuthenticationHandlerTest, WrongMessageReceived) {
  ExpectOnConnection();
  auth_handler_->HandleConnection(std::move(connection_));

  InputMessage* input_message;
  scoped_ptr<BlimpMessage> blimp_message = CreateBlimpMessage(&input_message);
  net::TestCompletionCallback process_message_cb;
  incoming_message_processor_->ProcessMessage(std::move(blimp_message),
                                              process_message_cb.callback());
  EXPECT_EQ(net::OK, process_message_cb.WaitForResult());
}

TEST_F(EngineAuthenticationHandlerTest, ConnectionError) {
  ExpectOnConnection();
  auth_handler_->HandleConnection(std::move(connection_));
  EXPECT_NE(nullptr, error_observer_);
  EXPECT_NE(nullptr, incoming_message_processor_);
  error_observer_->OnConnectionError(net::ERR_FAILED);
}

TEST_F(EngineAuthenticationHandlerTest, Timeout) {
  ExpectOnConnection();
  auth_handler_->HandleConnection(std::move(connection_));
  EXPECT_NE(nullptr, error_observer_);
  EXPECT_NE(nullptr, incoming_message_processor_);

  runner_->FastForwardBy(base::TimeDelta::FromSeconds(11));
}

TEST_F(EngineAuthenticationHandlerTest, AuthHandlerDeletedFirst) {
  ExpectOnConnection();
  auth_handler_->HandleConnection(std::move(connection_));
  auth_handler_.reset();

  scoped_ptr<BlimpMessage> blimp_message = CreateStartConnectionMessage("", 0);
  net::TestCompletionCallback process_message_cb;
  incoming_message_processor_->ProcessMessage(std::move(blimp_message),
                                              process_message_cb.callback());
  EXPECT_EQ(net::OK, process_message_cb.WaitForResult());
}

}  // namespace blimp
