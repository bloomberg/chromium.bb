// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/rpc/rpc_handler.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "components/copresence/handlers/directive_handler.h"
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

void AddMessageWithStrategy(ReportRequest* report,
                            BroadcastScanConfiguration strategy) {
  report->mutable_manage_messages_request()->add_message_to_publish()
      ->mutable_token_exchange_strategy()->set_broadcast_scan_configuration(
          strategy);
}

void AddSubscriptionWithStrategy(ReportRequest* report,
                                 BroadcastScanConfiguration strategy) {
  report->mutable_manage_subscriptions_request()->add_subscription()
      ->mutable_token_exchange_strategy()->set_broadcast_scan_configuration(
          strategy);
}

void CreateSubscribedMessage(const std::vector<std::string>& subscription_ids,
                             const std::string& message_string,
                             SubscribedMessage* message_proto) {
  message_proto->mutable_published_message()->set_payload(message_string);
  for (std::vector<std::string>::const_iterator subscription_id =
           subscription_ids.begin();
       subscription_id != subscription_ids.end();
       ++subscription_id) {
    message_proto->add_subscription_id(*subscription_id);
  }
}

// TODO(ckehoe): Make DirectiveHandler an interface.
class FakeDirectiveHandler : public DirectiveHandler {
 public:
  FakeDirectiveHandler() {}
  virtual ~FakeDirectiveHandler() {}

  const std::vector<Directive>& added_directives() const {
    return added_directives_;
  }

  virtual void Initialize(
      const AudioRecorder::DecodeSamplesCallback& decode_cb,
      const AudioDirectiveHandler::EncodeTokenCallback& encode_cb) OVERRIDE {}

  virtual void AddDirective(const Directive& directive) OVERRIDE {
    added_directives_.push_back(directive);
  }

  virtual void RemoveDirectives(const std::string& op_id) OVERRIDE {
    // TODO(ckehoe): Add a parallel implementation when prod has one.
  }

 private:
  std::vector<Directive> added_directives_;

  DISALLOW_COPY_AND_ASSIGN(FakeDirectiveHandler);
};

}  // namespace

class RpcHandlerTest : public testing::Test, public CopresenceClientDelegate {
 public:
  RpcHandlerTest() : rpc_handler_(this), status_(SUCCESS), api_key_("API key") {
    rpc_handler_.server_post_callback_ =
        base::Bind(&RpcHandlerTest::CaptureHttpPost, base::Unretained(this));
    rpc_handler_.device_id_ = "Device ID";
  }

  void CaptureHttpPost(
      net::URLRequestContextGetter* url_context_getter,
      const std::string& rpc_name,
      scoped_ptr<MessageLite> request_proto,
      const RpcHandler::PostCleanupCallback& response_callback) {
    rpc_name_ = rpc_name;
    request_proto_ = request_proto.Pass();
  }

  void CaptureStatus(CopresenceStatus status) {
    status_ = status;
  }

  inline const ReportRequest* GetReportSent() {
    return static_cast<ReportRequest*>(request_proto_.get());
  }

// TODO(ckehoe): Fix this on Windows. See rpc_handler.cc.
#ifndef OS_WIN
  const TokenTechnology& GetTokenTechnologyFromReport() {
    return GetReportSent()->update_signals_request().state().capabilities()
        .token_technology(0);
  }
#endif

  const RepeatedPtrField<PublishedMessage>& GetMessagesPublished() {
    return GetReportSent()->manage_messages_request().message_to_publish();
  }

  const RepeatedPtrField<Subscription>& GetSubscriptionsSent() {
    return GetReportSent()->manage_subscriptions_request().subscription();
  }

  void SetDeviceId(const std::string& device_id) {
    rpc_handler_.device_id_ = device_id;
  }

  const std::string& GetDeviceId() {
    return rpc_handler_.device_id_;
  }

  void AddInvalidToken(const std::string& token) {
    rpc_handler_.invalid_audio_token_cache_.Add(token, true);
  }

  bool TokenIsInvalid(const std::string& token) {
    return rpc_handler_.invalid_audio_token_cache_.HasKey(token);
  }

  FakeDirectiveHandler* InstallFakeDirectiveHandler() {
    FakeDirectiveHandler* handler = new FakeDirectiveHandler;
    rpc_handler_.directive_handler_.reset(handler);
    return handler;
  }

  void InvokeReportResponseHandler(int status_code,
                                   const std::string& response) {
    rpc_handler_.ReportResponseHandler(
        base::Bind(&RpcHandlerTest::CaptureStatus, base::Unretained(this)),
        NULL,
        status_code,
        response);
  }

  // CopresenceClientDelegate implementation

  virtual void HandleMessages(
      const std::string& app_id,
      const std::string& subscription_id,
      const std::vector<Message>& messages) OVERRIDE {
    // app_id is unused for now, pending a server fix.
    messages_by_subscription_[subscription_id] = messages;
  }

  virtual net::URLRequestContextGetter* GetRequestContext() const OVERRIDE {
    return NULL;
  }

  virtual const std::string GetPlatformVersionString() const OVERRIDE {
    return kChromeVersion;
  }

  virtual const std::string GetAPIKey() const OVERRIDE {
    return api_key_;
  }

  virtual WhispernetClient* GetWhispernetClient() OVERRIDE {
    return NULL;
  }

 protected:
  // For rpc_handler_.invalid_audio_token_cache_
  base::MessageLoop message_loop_;

  RpcHandler rpc_handler_;
  CopresenceStatus status_;
  std::string api_key_;

  std::string rpc_name_;
  scoped_ptr<MessageLite> request_proto_;
  std::map<std::string, std::vector<Message> > messages_by_subscription_;
};

TEST_F(RpcHandlerTest, Initialize) {
  SetDeviceId("");
  rpc_handler_.Initialize(RpcHandler::SuccessCallback());
  RegisterDeviceRequest* registration =
      static_cast<RegisterDeviceRequest*>(request_proto_.get());
  Identity identity = registration->device_identifiers().registrant();
  EXPECT_EQ(CHROME, identity.type());
  EXPECT_FALSE(identity.chrome_id().empty());
}

// TODO(ckehoe): Fix this on Windows. See rpc_handler.cc.
#ifndef OS_WIN

TEST_F(RpcHandlerTest, GetDeviceCapabilities) {
  // Empty request.
  rpc_handler_.SendReportRequest(make_scoped_ptr(new ReportRequest));
  EXPECT_EQ(RpcHandler::kReportRequestRpcName, rpc_name_);
  const TokenTechnology* token_technology = &GetTokenTechnologyFromReport();
  EXPECT_EQ(AUDIO_ULTRASOUND_PASSBAND, token_technology->medium());
  EXPECT_EQ(TRANSMIT, token_technology->instruction_type(0));
  EXPECT_EQ(RECEIVE, token_technology->instruction_type(1));

  // Request with broadcast only.
  scoped_ptr<ReportRequest> report(new ReportRequest);
  AddMessageWithStrategy(report.get(), BROADCAST_ONLY);
  rpc_handler_.SendReportRequest(report.Pass());
  token_technology = &GetTokenTechnologyFromReport();
  EXPECT_EQ(1, token_technology->instruction_type_size());
  EXPECT_EQ(TRANSMIT, token_technology->instruction_type(0));
  EXPECT_FALSE(GetReportSent()->has_manage_subscriptions_request());

  // Request with scan only.
  report.reset(new ReportRequest);
  AddSubscriptionWithStrategy(report.get(), SCAN_ONLY);
  AddSubscriptionWithStrategy(report.get(), SCAN_ONLY);
  rpc_handler_.SendReportRequest(report.Pass());
  token_technology = &GetTokenTechnologyFromReport();
  EXPECT_EQ(1, token_technology->instruction_type_size());
  EXPECT_EQ(RECEIVE, token_technology->instruction_type(0));
  EXPECT_FALSE(GetReportSent()->has_manage_messages_request());

  // Request with both scan and broadcast only (conflict).
  report.reset(new ReportRequest);
  AddMessageWithStrategy(report.get(), SCAN_ONLY);
  AddMessageWithStrategy(report.get(), BROADCAST_ONLY);
  AddSubscriptionWithStrategy(report.get(), BROADCAST_ONLY);
  rpc_handler_.SendReportRequest(report.Pass());
  token_technology = &GetTokenTechnologyFromReport();
  EXPECT_EQ(TRANSMIT, token_technology->instruction_type(0));
  EXPECT_EQ(RECEIVE, token_technology->instruction_type(1));

  // Request with broadcast and scan.
  report.reset(new ReportRequest);
  AddMessageWithStrategy(report.get(), SCAN_ONLY);
  AddSubscriptionWithStrategy(report.get(), BROADCAST_AND_SCAN);
  rpc_handler_.SendReportRequest(report.Pass());
  token_technology = &GetTokenTechnologyFromReport();
  EXPECT_EQ(TRANSMIT, token_technology->instruction_type(0));
  EXPECT_EQ(RECEIVE, token_technology->instruction_type(1));
}
#endif

TEST_F(RpcHandlerTest, AllowOptedOutMessages) {
  // Request with no filter specified.
  scoped_ptr<ReportRequest> report(new ReportRequest);
  report->mutable_manage_messages_request()->add_message_to_publish()
      ->set_id("message");
  report->mutable_manage_subscriptions_request()->add_subscription()
      ->set_id("subscription");
  rpc_handler_.SendReportRequest(report.Pass());
  const OptInStateFilter& filter =
      GetMessagesPublished().Get(0).opt_in_state_filter();
  ASSERT_EQ(2, filter.allowed_opt_in_state_size());
  EXPECT_EQ(OPTED_IN, filter.allowed_opt_in_state(0));
  EXPECT_EQ(OPTED_OUT, filter.allowed_opt_in_state(1));
  EXPECT_EQ(2, GetSubscriptionsSent().Get(0).opt_in_state_filter()
      .allowed_opt_in_state_size());

  // Request with filters already specified.
  report.reset(new ReportRequest);
  report->mutable_manage_messages_request()->add_message_to_publish()
      ->mutable_opt_in_state_filter()->add_allowed_opt_in_state(OPTED_IN);
  report->mutable_manage_subscriptions_request()->add_subscription()
      ->mutable_opt_in_state_filter()->add_allowed_opt_in_state(OPTED_OUT);
  rpc_handler_.SendReportRequest(report.Pass());
  const OptInStateFilter& publish_filter =
      GetMessagesPublished().Get(0).opt_in_state_filter();
  ASSERT_EQ(1, publish_filter.allowed_opt_in_state_size());
  EXPECT_EQ(OPTED_IN, publish_filter.allowed_opt_in_state(0));
  const OptInStateFilter& subscription_filter =
      GetSubscriptionsSent().Get(0).opt_in_state_filter();
  ASSERT_EQ(1, subscription_filter.allowed_opt_in_state_size());
  EXPECT_EQ(OPTED_OUT, subscription_filter.allowed_opt_in_state(0));
}

TEST_F(RpcHandlerTest, CreateRequestHeader) {
  SetDeviceId("CreateRequestHeader Device ID");
  rpc_handler_.SendReportRequest(make_scoped_ptr(new ReportRequest),
                                 "CreateRequestHeader App ID",
                                 StatusCallback());
  EXPECT_EQ(RpcHandler::kReportRequestRpcName, rpc_name_);
  ReportRequest* report = static_cast<ReportRequest*>(request_proto_.get());
  EXPECT_EQ(kChromeVersion,
            report->header().framework_version().version_name());
  EXPECT_EQ("CreateRequestHeader App ID",
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

  rpc_handler_.ReportTokens(test_tokens);
  EXPECT_EQ(RpcHandler::kReportRequestRpcName, rpc_name_);
  ReportRequest* report = static_cast<ReportRequest*>(request_proto_.get());
  google::protobuf::RepeatedPtrField<TokenObservation> tokens_sent =
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
