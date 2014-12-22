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
#include "components/copresence/copresence_state_impl.h"
#include "components/copresence/handlers/directive_handler.h"
#include "components/copresence/mediums/audio/audio_manager.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/proto/enums.pb.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/test/fake_directive_handler.h"
#include "components/copresence/test/stub_whispernet_client.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

using google::protobuf::MessageLite;
using google::protobuf::RepeatedPtrField;

using testing::ElementsAre;
using testing::Property;
using testing::SizeIs;

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

}  // namespace

class RpcHandlerTest : public testing::Test, public CopresenceDelegate {
 public:
  RpcHandlerTest()
      : whispernet_client_(new StubWhispernetClient),
        // TODO(ckehoe): Use a FakeCopresenceState here
        // and test that it gets called correctly.
        state_(new CopresenceStateImpl),
        rpc_handler_(this,
                     state_.get(),
                     &directive_handler_,
                     nullptr,
                     base::Bind(&RpcHandlerTest::CaptureHttpPost,
                                base::Unretained(this))),
        status_(SUCCESS) {}

  // CopresenceDelegate implementation

  void HandleMessages(const std::string& /* app_id */,
                      const std::string& subscription_id,
                      const std::vector<Message>& messages) override {
    // app_id is unused for now, pending a server fix.
    for (const Message& message : messages) {
      messages_by_subscription_[subscription_id].push_back(message.payload());
    }
  }

  void HandleStatusUpdate(CopresenceStatus /* status */) override {}

  net::URLRequestContextGetter* GetRequestContext() const override {
    return nullptr;
  }

  const std::string GetPlatformVersionString() const override {
    return kChromeVersion;
  }

  const std::string GetAPIKey(const std::string& app_id) const override {
    return app_id + " API Key";
  }

  WhispernetClient* GetWhispernetClient() override {
    return whispernet_client_.get();
  }

  // TODO(ckehoe): Add GCM tests.
  gcm::GCMDriver* GetGCMDriver() override {
    return nullptr;
  }

 protected:

  // Send test input to RpcHandler

  void RegisterForToken(const std::string& auth_token) {
    rpc_handler_.RegisterForToken(auth_token);
  }

  void SendRegisterResponse(const std::string& auth_token,
                            const std::string& device_id) {
    RegisterDeviceResponse response;
    response.set_registered_device_id(device_id);
    response.mutable_header()->mutable_status()->set_code(OK);

    std::string serialized_response;
    response.SerializeToString(&serialized_response);
    rpc_handler_.RegisterResponseHandler(
        auth_token, false, nullptr, net::HTTP_OK, serialized_response);
  }

  void SendReport(scoped_ptr<ReportRequest> request,
                  const std::string& app_id,
                  const std::string& auth_token) {
    rpc_handler_.SendReportRequest(
      request.Pass(), app_id, auth_token, StatusCallback());
  }

  void SendReportResponse(int status_code,
                          scoped_ptr<ReportResponse> response) {
    response->mutable_header()->mutable_status()->set_code(OK);

    std::string serialized_response;
    response->SerializeToString(&serialized_response);
    rpc_handler_.ReportResponseHandler(
        base::Bind(&RpcHandlerTest::CaptureStatus, base::Unretained(this)),
        nullptr,
        status_code,
        serialized_response);
  }

  // Read and modify RpcHandler state

  const ScopedVector<RpcHandler::PendingRequest>& request_queue() const {
    return rpc_handler_.pending_requests_queue_;
  }

  std::map<std::string, std::string>& device_id_by_auth_token() {
    return rpc_handler_.device_id_by_auth_token_;
  }

  void AddInvalidToken(const std::string& token) {
    rpc_handler_.invalid_audio_token_cache_.Add(token, true);
  }

  bool TokenIsInvalid(const std::string& token) {
    return rpc_handler_.invalid_audio_token_cache_.HasKey(token);
  }

  // For rpc_handler_.invalid_audio_token_cache_
  base::MessageLoop message_loop_;

  scoped_ptr<WhispernetClient> whispernet_client_;
  FakeDirectiveHandler directive_handler_;
  scoped_ptr<CopresenceStateImpl> state_;
  RpcHandler rpc_handler_;

  CopresenceStatus status_;
  std::string rpc_name_;
  std::string api_key_;
  std::string auth_token_;
  ScopedVector<MessageLite> request_protos_;
  std::map<std::string, std::vector<std::string>> messages_by_subscription_;

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

TEST_F(RpcHandlerTest, RegisterForToken) {
  RegisterForToken("");
  EXPECT_THAT(request_protos_, SizeIs(1));
  const RegisterDeviceRequest* registration =
      static_cast<RegisterDeviceRequest*>(request_protos_[0]);
  Identity identity = registration->device_identifiers().registrant();
  EXPECT_EQ(CHROME, identity.type());
  EXPECT_FALSE(identity.chrome_id().empty());

  RegisterForToken("abc");
  EXPECT_THAT(request_protos_, SizeIs(2));
  registration = static_cast<RegisterDeviceRequest*>(request_protos_[1]);
  EXPECT_FALSE(registration->has_device_identifiers());
}

TEST_F(RpcHandlerTest, RequestQueuing) {
  // Send a report.
  ReportRequest* report = new ReportRequest;
  report->mutable_manage_messages_request()->add_id_to_unpublish("unpublish");
  SendReport(make_scoped_ptr(report), "Q App ID", "Q Auth Token");
  EXPECT_THAT(request_queue(), SizeIs(1));
  EXPECT_EQ("Q Auth Token", request_queue()[0]->auth_token);

  // Check for registration request.
  EXPECT_THAT(request_protos_, SizeIs(1));
  const RegisterDeviceRequest* registration =
      static_cast<RegisterDeviceRequest*>(request_protos_[0]);
  EXPECT_FALSE(registration->device_identifiers().has_registrant());
  EXPECT_EQ("Q Auth Token", auth_token_);

  // Send a second report.
  report = new ReportRequest;
  report->mutable_manage_subscriptions_request()->add_id_to_unsubscribe(
      "unsubscribe");
  SendReport(make_scoped_ptr(report), "Q App ID", "Q Auth Token");
  EXPECT_THAT(request_protos_, SizeIs(1));
  EXPECT_THAT(request_queue(), SizeIs(2));
  EXPECT_EQ("Q Auth Token", request_queue()[1]->auth_token);

  // Send an anonymous report.
  report = new ReportRequest;
  report->mutable_update_signals_request()->add_token_observation()
      ->set_token_id("Q Audio Token");
  SendReport(make_scoped_ptr(report), "Q App ID", "");
  EXPECT_THAT(request_queue(), SizeIs(3));
  EXPECT_EQ("", request_queue()[2]->auth_token);

  // Check for another registration request.
  EXPECT_THAT(request_protos_, SizeIs(2));
  registration = static_cast<RegisterDeviceRequest*>(request_protos_[1]);
  EXPECT_TRUE(registration->device_identifiers().has_registrant());
  EXPECT_EQ("", auth_token_);

  // Respond to the first registration.
  SendRegisterResponse("Q Auth Token", "Q Auth Device ID");
  EXPECT_EQ("Q Auth Device ID", device_id_by_auth_token()["Q Auth Token"]);

  // Check that queued reports are sent.
  EXPECT_THAT(request_protos_, SizeIs(4));
  EXPECT_THAT(request_queue(), SizeIs(1));
  EXPECT_THAT(directive_handler_.removed_directives(),
              ElementsAre("unpublish", "unsubscribe"));
  report = static_cast<ReportRequest*>(request_protos_[2]);
  EXPECT_EQ("unpublish", report->manage_messages_request().id_to_unpublish(0));
  report = static_cast<ReportRequest*>(request_protos_[3]);
  EXPECT_EQ("unsubscribe",
            report->manage_subscriptions_request().id_to_unsubscribe(0));

  // Respond to the second registration.
  SendRegisterResponse("", "Q Anonymous Device ID");
  EXPECT_EQ("Q Anonymous Device ID", device_id_by_auth_token()[""]);

  // Check for last report.
  EXPECT_THAT(request_protos_, SizeIs(5));
  EXPECT_TRUE(request_queue().empty());
  report = static_cast<ReportRequest*>(request_protos_[4]);
  EXPECT_EQ("Q Audio Token",
            report->update_signals_request().token_observation(0).token_id());
}

TEST_F(RpcHandlerTest, CreateRequestHeader) {
  device_id_by_auth_token()["CreateRequestHeader Auth Token"] =
      "CreateRequestHeader Device ID";
  SendReport(make_scoped_ptr(new ReportRequest),
             "CreateRequestHeader App",
             "CreateRequestHeader Auth Token");

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
  test_tokens.push_back(AudioToken("token 2", false));
  test_tokens.push_back(AudioToken("token 3", true));
  AddInvalidToken("token 2");

  device_id_by_auth_token()[""] = "ReportTokens Anonymous Device";
  device_id_by_auth_token()["ReportTokens Auth"] = "ReportTokens Auth Device";

  rpc_handler_.ReportTokens(test_tokens);
  EXPECT_EQ(RpcHandler::kReportRequestRpcName, rpc_name_);
  EXPECT_EQ(" API Key", api_key_);
  EXPECT_THAT(request_protos_, SizeIs(2));
  const ReportRequest* report = static_cast<ReportRequest*>(request_protos_[0]);
  RepeatedPtrField<TokenObservation> tokens_sent =
      report->update_signals_request().token_observation();
  EXPECT_THAT(tokens_sent, ElementsAre(
      Property(&TokenObservation::token_id, "token 1"),
      Property(&TokenObservation::token_id, "token 3"),
      Property(&TokenObservation::token_id, "current audible"),
      Property(&TokenObservation::token_id, "current inaudible")));
}

TEST_F(RpcHandlerTest, ReportResponseHandler) {
  // Fail on HTTP status != 200.
  scoped_ptr<ReportResponse> response(new ReportResponse);
  status_ = SUCCESS;
  SendReportResponse(net::HTTP_BAD_REQUEST, response.Pass());
  EXPECT_EQ(FAIL, status_);

  // Construct test subscriptions.
  std::vector<std::string> subscription_1(1, "Subscription 1");
  std::vector<std::string> subscription_2(1, "Subscription 2");
  std::vector<std::string> both_subscriptions;
  both_subscriptions.push_back("Subscription 1");
  both_subscriptions.push_back("Subscription 2");

  // Construct a test ReportResponse.
  response.reset(new ReportResponse);
  response->mutable_header()->mutable_status()->set_code(OK);
  UpdateSignalsResponse* update_response =
      response->mutable_update_signals_response();
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

  // Process it.
  messages_by_subscription_.clear();
  status_ = FAIL;
  SendReportResponse(net::HTTP_OK, response.Pass());

  // Check processing.
  EXPECT_EQ(SUCCESS, status_);
  EXPECT_TRUE(TokenIsInvalid("bad token"));
  EXPECT_THAT(messages_by_subscription_["Subscription 1"],
              ElementsAre("Message A", "Message C"));
  EXPECT_THAT(messages_by_subscription_["Subscription 2"],
              ElementsAre("Message B", "Message C"));
  EXPECT_THAT(directive_handler_.added_directives(),
              ElementsAre("Subscription 1", "Subscription 2"));
}

}  // namespace copresence
