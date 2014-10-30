// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/rpc/rpc_handler.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "components/copresence/handlers/directive_handler.h"
#include "components/copresence/mediums/audio/audio_manager.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/proto/enums.pb.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

using google::protobuf::MessageLite;
using google::protobuf::RepeatedPtrField;

namespace copresence {

namespace {

const char kChromeVersion[] = "Chrome Version String";

void CreateSubscribedMessage(const std::vector<std::string>& subscription_ids,
                             const std::string& message_string,
                             SubscribedMessage* message_proto) {
  message_proto->mutable_published_message()->set_payload(message_string);
  for (const std::string& subscription_id : subscription_ids) {
    message_proto->add_subscription_id(subscription_id);
  }
}

// TODO(ckehoe): Make DirectiveHandler an interface.
class FakeDirectiveHandler : public DirectiveHandler {
 public:
  FakeDirectiveHandler() {}
  ~FakeDirectiveHandler() override {}

  const std::vector<Directive>& added_directives() const {
    return added_directives_;
  }

  void Initialize(const AudioManager::DecodeSamplesCallback& decode_cb,
                  const AudioManager::EncodeTokenCallback& encode_cb) override {
  }

  void AddDirective(const Directive& directive) override {
    added_directives_.push_back(directive);
  }

  void RemoveDirectives(const std::string& op_id) override {
    // TODO(ckehoe): Add a parallel implementation when prod has one.
  }

 private:
  std::vector<Directive> added_directives_;

  DISALLOW_COPY_AND_ASSIGN(FakeDirectiveHandler);
};

}  // namespace

class RpcHandlerTest : public testing::Test, public CopresenceDelegate {
 public:
  RpcHandlerTest() : rpc_handler_(this), status_(SUCCESS) {
    rpc_handler_.server_post_callback_ =
        base::Bind(&RpcHandlerTest::CaptureHttpPost, base::Unretained(this));
  }

  // CopresenceDelegate implementation

  void HandleMessages(const std::string& app_id,
                      const std::string& subscription_id,
                      const std::vector<Message>& messages) override {
    // app_id is unused for now, pending a server fix.
    messages_by_subscription_[subscription_id] = messages;
  }

  net::URLRequestContextGetter* GetRequestContext() const override {
    return NULL;
  }

  const std::string GetPlatformVersionString() const override {
    return kChromeVersion;
  }

  const std::string GetAPIKey(const std::string& app_id) const override {
    return app_id + " API Key";
  }

  const std::string GetAuthToken() const override {
    return auth_token_;
  }

  WhispernetClient* GetWhispernetClient() override { return NULL; }

 protected:
  void InvokeReportResponseHandler(int status_code,
                                   const std::string& response) {
    rpc_handler_.ReportResponseHandler(
        base::Bind(&RpcHandlerTest::CaptureStatus, base::Unretained(this)),
        NULL,
        status_code,
        response);
  }

  FakeDirectiveHandler* InstallFakeDirectiveHandler() {
    FakeDirectiveHandler* handler = new FakeDirectiveHandler;
    rpc_handler_.directive_handler_.reset(handler);
    return handler;
  }

  void SetDeviceIdAndAuthToken(const std::string& device_id,
                               const std::string& auth_token) {
    rpc_handler_.device_id_by_auth_token_[auth_token] = device_id;
    auth_token_ = auth_token;
  }

  void AddInvalidToken(const std::string& token) {
    rpc_handler_.invalid_audio_token_cache_.Add(token, true);
  }

  bool TokenIsInvalid(const std::string& token) {
    return rpc_handler_.invalid_audio_token_cache_.HasKey(token);
  }

  // For rpc_handler_.invalid_audio_token_cache_
  base::MessageLoop message_loop_;

  RpcHandler rpc_handler_;
  CopresenceStatus status_;

  std::string rpc_name_;
  std::string api_key_;
  std::string auth_token_;
  ScopedVector<MessageLite> request_protos_;
  std::map<std::string, std::vector<Message>> messages_by_subscription_;

 private:
  void CaptureHttpPost(
      net::URLRequestContextGetter* url_context_getter,
      const std::string& rpc_name,
      const std::string& api_key,
      const std::string& auth_token,
      scoped_ptr<MessageLite> request_proto,
      const RpcHandler::PostCleanupCallback& response_callback) {
    rpc_name_ = rpc_name;
    api_key_ = api_key;
    auth_token_ = auth_token;
    request_protos_.push_back(request_proto.release());
  }

  void CaptureStatus(CopresenceStatus status) {
    status_ = status;
  }
};

TEST_F(RpcHandlerTest, RegisterDevice) {
  EXPECT_FALSE(rpc_handler_.IsRegisteredForToken(""));
  rpc_handler_.RegisterForToken("", RpcHandler::SuccessCallback());
  EXPECT_EQ(1u, request_protos_.size());
  const RegisterDeviceRequest* registration =
      static_cast<RegisterDeviceRequest*>(request_protos_[0]);
  Identity identity = registration->device_identifiers().registrant();
  EXPECT_EQ(CHROME, identity.type());
  EXPECT_FALSE(identity.chrome_id().empty());

  EXPECT_FALSE(rpc_handler_.IsRegisteredForToken("abc"));
  rpc_handler_.RegisterForToken("abc", RpcHandler::SuccessCallback());
  EXPECT_EQ(2u, request_protos_.size());
  registration = static_cast<RegisterDeviceRequest*>(request_protos_[1]);
  EXPECT_FALSE(registration->has_device_identifiers());
}

// TODO(ckehoe): Add a test for RegisterResponseHandler.

TEST_F(RpcHandlerTest, CreateRequestHeader) {
  SetDeviceIdAndAuthToken("CreateRequestHeader Device ID",
                          "CreateRequestHeader Auth Token");
  rpc_handler_.SendReportRequest(make_scoped_ptr(new ReportRequest),
                                 "CreateRequestHeader App",
                                 "CreateRequestHeader Auth Token",
                                 StatusCallback());
  EXPECT_EQ(RpcHandler::kReportRequestRpcName, rpc_name_);
  EXPECT_EQ("CreateRequestHeader App API Key", api_key_);
  EXPECT_EQ("CreateRequestHeader Auth Token", auth_token_);
  const ReportRequest* report = static_cast<ReportRequest*>(request_protos_[0]);
  EXPECT_EQ(kChromeVersion,
            report->header().framework_version().version_name());
  EXPECT_EQ("CreateRequestHeader App",
            report->header().client_version().client());
  EXPECT_EQ("CreateRequestHeader Device ID",
            report->header().registered_device_id());
  EXPECT_EQ(CHROME_PLATFORM_TYPE,
            report->header().device_fingerprint().type());
}

TEST_F(RpcHandlerTest, ReportTokens) {
  std::vector<AudioToken> test_tokens;
  test_tokens.push_back(AudioToken("token 1", false));
  test_tokens.push_back(AudioToken("token 2", true));
  test_tokens.push_back(AudioToken("token 3", false));
  AddInvalidToken("token 2");

  SetDeviceIdAndAuthToken("ReportTokens Device 1", "");
  SetDeviceIdAndAuthToken("ReportTokens Device 2", "ReportTokens Auth");

  rpc_handler_.ReportTokens(test_tokens);
  EXPECT_EQ(RpcHandler::kReportRequestRpcName, rpc_name_);
  EXPECT_EQ(" API Key", api_key_);
  EXPECT_EQ(2u, request_protos_.size());
  const ReportRequest* report = static_cast<ReportRequest*>(request_protos_[0]);
  RepeatedPtrField<TokenObservation> tokens_sent =
      report->update_signals_request().token_observation();
  ASSERT_EQ(2, tokens_sent.size());
  EXPECT_EQ("token 1", tokens_sent.Get(0).token_id());
  EXPECT_EQ("token 3", tokens_sent.Get(1).token_id());
}

TEST_F(RpcHandlerTest, ReportResponseHandler) {
  // Fail on HTTP status != 200.
  ReportResponse empty_response;
  empty_response.mutable_header()->mutable_status()->set_code(OK);
  std::string serialized_empty_response;
  ASSERT_TRUE(empty_response.SerializeToString(&serialized_empty_response));
  status_ = SUCCESS;
  InvokeReportResponseHandler(net::HTTP_BAD_REQUEST, serialized_empty_response);
  EXPECT_EQ(FAIL, status_);

  std::vector<std::string> subscription_1(1, "Subscription 1");
  std::vector<std::string> subscription_2(1, "Subscription 2");
  std::vector<std::string> both_subscriptions;
  both_subscriptions.push_back("Subscription 1");
  both_subscriptions.push_back("Subscription 2");

  ReportResponse test_response;
  test_response.mutable_header()->mutable_status()->set_code(OK);
  UpdateSignalsResponse* update_response =
      test_response.mutable_update_signals_response();
  update_response->set_status(util::error::OK);
  Token* invalid_token = update_response->add_token();
  invalid_token->set_id("bad token");
  invalid_token->set_status(INVALID);
  CreateSubscribedMessage(
      subscription_1, "Message A", update_response->add_message());
  CreateSubscribedMessage(
      subscription_2, "Message B", update_response->add_message());
  CreateSubscribedMessage(
      both_subscriptions, "Message C", update_response->add_message());
  update_response->add_directive()->set_subscription_id("Subscription 1");
  update_response->add_directive()->set_subscription_id("Subscription 2");

  messages_by_subscription_.clear();
  FakeDirectiveHandler* directive_handler = InstallFakeDirectiveHandler();
  std::string serialized_proto;
  ASSERT_TRUE(test_response.SerializeToString(&serialized_proto));
  status_ = FAIL;
  InvokeReportResponseHandler(net::HTTP_OK, serialized_proto);

  EXPECT_EQ(SUCCESS, status_);
  EXPECT_TRUE(TokenIsInvalid("bad token"));
  ASSERT_EQ(2U, messages_by_subscription_.size());
  ASSERT_EQ(2U, messages_by_subscription_["Subscription 1"].size());
  ASSERT_EQ(2U, messages_by_subscription_["Subscription 2"].size());
  EXPECT_EQ("Message A",
            messages_by_subscription_["Subscription 1"][0].payload());
  EXPECT_EQ("Message B",
            messages_by_subscription_["Subscription 2"][0].payload());
  EXPECT_EQ("Message C",
            messages_by_subscription_["Subscription 1"][1].payload());
  EXPECT_EQ("Message C",
            messages_by_subscription_["Subscription 2"][1].payload());

  ASSERT_EQ(2U, directive_handler->added_directives().size());
  EXPECT_EQ("Subscription 1",
            directive_handler->added_directives()[0].subscription_id());
  EXPECT_EQ("Subscription 2",
            directive_handler->added_directives()[1].subscription_id());
}

}  // namespace copresence
