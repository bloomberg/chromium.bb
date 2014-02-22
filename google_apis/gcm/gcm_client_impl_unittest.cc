// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/gcm_client_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "components/encryptor/encryptor.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/fake_connection_factory.h"
#include "google_apis/gcm/engine/fake_connection_handler.h"
#include "google_apis/gcm/protocol/android_checkin.pb.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

enum LastEvent {
  NONE,
  LOADING_COMPLETED,
  REGISTRATION_COMPLETED,
  UNREGISTRATION_COMPLETED,
  MESSAGE_SEND_ERROR,
  MESSAGE_RECEIVED,
  MESSAGES_DELETED,
};

const uint64 kDeviceAndroidId = 54321;
const uint64 kDeviceSecurityToken = 12345;
const char kRegistrationResponsePrefix[] = "token=";
const char kUnregistrationResponsePrefix[] = "deleted=";

// Helper for building arbitrary data messages.
MCSMessage BuildDownstreamMessage(
    const std::string& project_id,
    const std::string& app_id,
    const std::map<std::string, std::string>& data) {
  mcs_proto::DataMessageStanza data_message;
  data_message.set_from(project_id);
  data_message.set_category(app_id);
  for (std::map<std::string, std::string>::const_iterator iter = data.begin();
       iter != data.end();
       ++iter) {
    mcs_proto::AppData* app_data = data_message.add_app_data();
    app_data->set_key(iter->first);
    app_data->set_value(iter->second);
  }
  return MCSMessage(kDataMessageStanzaTag, data_message);
}

class FakeMCSClient : public MCSClient {
 public:
  FakeMCSClient(base::Clock* clock,
                ConnectionFactory* connection_factory);
  virtual ~FakeMCSClient();
  virtual void Login(uint64 android_id, uint64 security_token) OVERRIDE;
  virtual void SendMessage(const MCSMessage& message) OVERRIDE;
  void set_gcm_store(GCMStore* gcm_store);

  uint64 last_android_id() const { return last_android_id_; }
  uint64 last_security_token() const { return last_security_token_; }
  uint8 last_message_tag() const { return last_message_tag_; }
  const mcs_proto::DataMessageStanza& last_data_message_stanza() const {
    return last_data_message_stanza_;
  }

 private:
  uint64 last_android_id_;
  uint64 last_security_token_;
  uint8 last_message_tag_;
  mcs_proto::DataMessageStanza last_data_message_stanza_;
};

FakeMCSClient::FakeMCSClient(base::Clock* clock,
                             ConnectionFactory* connection_factory)
    : MCSClient("", clock, connection_factory, NULL),
      last_android_id_(0u),
      last_security_token_(0u),
      last_message_tag_(kNumProtoTypes) {
}

FakeMCSClient::~FakeMCSClient() {
}

void FakeMCSClient::set_gcm_store(GCMStore* gcm_store) {
  SetGCMStoreForTesting(gcm_store);
}

void FakeMCSClient::Login(uint64 android_id, uint64 security_token) {
  last_android_id_ = android_id;
  last_security_token_ = security_token;
}

void FakeMCSClient::SendMessage(const MCSMessage& message) {
  last_message_tag_ = message.tag();
  if (last_message_tag_ == kDataMessageStanzaTag) {
    last_data_message_stanza_.CopyFrom(
        reinterpret_cast<const mcs_proto::DataMessageStanza&>(
            message.GetProtobuf()));
  }
}

}  // namespace

class GCMClientImplTest : public testing::Test,
                          public GCMClient::Delegate {
 public:
  GCMClientImplTest();
  virtual ~GCMClientImplTest();

  virtual void SetUp() OVERRIDE;

  void BuildGCMClient();
  void InitializeGCMClient();
  void ReceiveMessageFromMCS(const MCSMessage& message);
  void CompleteCheckin(uint64 android_id, uint64 security_token);
  void CompleteRegistration(const std::string& registration_id);
  void CompleteUnregistration(const std::string& app_id);

  // GCMClient::Delegate overrides (for verification).
  virtual void OnRegisterFinished(const std::string& app_id,
                                  const std::string& registration_id,
                                  GCMClient::Result result) OVERRIDE;
  virtual void OnUnregisterFinished(const std::string& app_id,
                                    bool success) OVERRIDE;
  virtual void OnSendFinished(const std::string& app_id,
                              const std::string& message_id,
                              GCMClient::Result result) OVERRIDE {}
  virtual void OnMessageReceived(const std::string& registration_id,
                                 const GCMClient::IncomingMessage& message)
      OVERRIDE;
  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE;
  virtual void OnMessageSendError(const std::string& app_id,
                                  const std::string& message_id,
                                  GCMClient::Result result) OVERRIDE;
  virtual void OnGCMReady() OVERRIDE;

  GCMClientImpl* gcm_client() const { return gcm_client_.get(); }
  FakeMCSClient* mcs_client() const {
    return reinterpret_cast<FakeMCSClient*>(gcm_client_->mcs_client_.get());
  }

  LastEvent last_event() const { return last_event_; }
  const std::string& last_app_id() const { return last_app_id_; }
  const std::string& last_registration_id() const {
    return last_registration_id_;
  }
  const std::string& last_message_id() const { return last_message_id_; }
  GCMClient::Result last_result() const { return last_result_; }
  const GCMClient::IncomingMessage& last_message() const {
    return last_message_;
  }

  int64 CurrentTime();

 private:
  // Tooling.
  void PumpLoop();
  void PumpLoopUntilIdle();
  void QuitLoop();

  base::SimpleTestClock* clock() const {
    return reinterpret_cast<base::SimpleTestClock*>(gcm_client_->clock_.get());
  }

  // Variables used for verification.
  LastEvent last_event_;
  std::string last_app_id_;
  std::string last_registration_id_;
  std::string last_message_id_;
  GCMClient::Result last_result_;
  GCMClient::IncomingMessage last_message_;

  scoped_ptr<GCMClientImpl> gcm_client_;
  scoped_ptr<FakeConnectionFactory> connection_factory_;

  base::MessageLoop message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  net::TestURLFetcherFactory url_fetcher_factory_;

  // Injected to GCM client:
  base::ScopedTempDir temp_directory_;
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
};

GCMClientImplTest::GCMClientImplTest()
    : last_event_(NONE),
      last_result_(GCMClient::UNKNOWN_ERROR),
      url_request_context_getter_(new net::TestURLRequestContextGetter(
          message_loop_.message_loop_proxy())) {
}

GCMClientImplTest::~GCMClientImplTest() {}

void GCMClientImplTest::SetUp() {
  ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
  run_loop_.reset(new base::RunLoop);
  BuildGCMClient();
  InitializeGCMClient();
}

void GCMClientImplTest::PumpLoop() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void GCMClientImplTest::PumpLoopUntilIdle() {
  run_loop_->RunUntilIdle();
  run_loop_.reset(new base::RunLoop());
}

void GCMClientImplTest::QuitLoop() {
  if (run_loop_ && run_loop_->running())
    run_loop_->Quit();
}

void GCMClientImplTest::BuildGCMClient() {
  gcm_client_.reset(new GCMClientImpl());
}

void GCMClientImplTest::CompleteCheckin(uint64 android_id,
                                        uint64 security_token) {
  checkin_proto::AndroidCheckinResponse response;
  response.set_stats_ok(true);
  response.set_android_id(android_id);
  response.set_security_token(security_token);

  std::string response_string;
  response.SerializeToString(&response_string);

  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(response_string);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  url_fetcher_factory_.RemoveFetcherFromMap(0);
}

void GCMClientImplTest::CompleteRegistration(
    const std::string& registration_id) {
  std::string response(kRegistrationResponsePrefix);
  response.append(registration_id);
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(response);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void GCMClientImplTest::CompleteUnregistration(
    const std::string& app_id) {
  std::string response(kUnregistrationResponsePrefix);
  response.append(app_id);
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(response);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void GCMClientImplTest::InitializeGCMClient() {
  // Creating and advancing the clock.
  gcm_client_->clock_.reset(new base::SimpleTestClock);
  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  // Creating and injecting the mcs_client.
  connection_factory_.reset(new FakeConnectionFactory());
  gcm_client_->SetMCSClientForTesting(scoped_ptr<MCSClient>(
      new FakeMCSClient(clock(), connection_factory_.get())).Pass());
  // Actual initialization.
  checkin_proto::ChromeBuildProto chrome_build_proto;
  gcm_client_->Initialize(chrome_build_proto,
                          temp_directory_.path(),
                          std::vector<std::string>(),
                          message_loop_.message_loop_proxy(),
                          url_request_context_getter_,
                          this);
#if defined(OS_MACOSX)
  // On OSX, prevent the Keychain permissions popup during unit tests.
  Encryptor::UseMockKeychain(true);  // Must be after Initialize.
#endif
  // Ensuring that mcs_client is using the same gcm_store as gcm_client.
  mcs_client()->set_gcm_store(gcm_client_->gcm_store_.get());
  PumpLoopUntilIdle();
  CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken);
}

void GCMClientImplTest::ReceiveMessageFromMCS(const MCSMessage& message) {
  gcm_client_->OnMessageReceivedFromMCS(message);
}

void GCMClientImplTest::OnGCMReady() {
  last_event_ = LOADING_COMPLETED;
  QuitLoop();
}

void GCMClientImplTest::OnMessageReceived(
    const std::string& registration_id,
    const GCMClient::IncomingMessage& message) {
  last_event_ = MESSAGE_RECEIVED;
  last_app_id_ = registration_id;
  last_message_ = message;
  QuitLoop();
}

void GCMClientImplTest::OnRegisterFinished(const std::string& app_id,
                                           const std::string& registration_id,
                                           GCMClient::Result result) {
  last_event_ = REGISTRATION_COMPLETED;
  last_app_id_ = app_id;
  last_registration_id_ = registration_id;
  last_result_ = result;
}

void GCMClientImplTest::OnUnregisterFinished(const std::string& app_id,
                                             bool success) {
  last_event_ = UNREGISTRATION_COMPLETED;
  last_app_id_ = app_id;
  last_result_ = success ? GCMClient::SUCCESS : GCMClient::UNKNOWN_ERROR;
}

void GCMClientImplTest::OnMessagesDeleted(const std::string& app_id) {
  last_event_ = MESSAGES_DELETED;
  last_app_id_ = app_id;
}

void GCMClientImplTest::OnMessageSendError(const std::string& app_id,
                                           const std::string& message_id,
                                           GCMClient::Result result) {
  last_event_ = MESSAGE_SEND_ERROR;
  last_app_id_ = app_id;
  last_message_id_ = message_id;
  last_result_ = result;
}

int64 GCMClientImplTest::CurrentTime() {
  return clock()->Now().ToInternalValue() / base::Time::kMicrosecondsPerSecond;
}

TEST_F(GCMClientImplTest, LoadingCompleted) {
  EXPECT_EQ(LOADING_COMPLETED, last_event());
  EXPECT_EQ(kDeviceAndroidId, mcs_client()->last_android_id());
  EXPECT_EQ(kDeviceSecurityToken, mcs_client()->last_security_token());
}

TEST_F(GCMClientImplTest, CheckOut) {
  EXPECT_TRUE(mcs_client());
  gcm_client()->CheckOut();
  EXPECT_FALSE(mcs_client());
}

TEST_F(GCMClientImplTest, RegisterApp) {
  std::vector<std::string> senders;
  senders.push_back("sender");
  gcm_client()->Register("app_id", "cert", senders);
  CompleteRegistration("reg_id");

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ("app_id", last_app_id());
  EXPECT_EQ("reg_id", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
}

TEST_F(GCMClientImplTest, UnregisterApp) {
  gcm_client()->Unregister("app_id");
  CompleteUnregistration("app_id");

  EXPECT_EQ(UNREGISTRATION_COMPLETED, last_event());
  EXPECT_EQ("app_id", last_app_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
}

TEST_F(GCMClientImplTest, DispatchDownstreamMessage) {
  std::map<std::string, std::string> expected_data;
  expected_data["message_type"] = "gcm";
  expected_data["key"] = "value";
  expected_data["key2"] = "value2";
  MCSMessage message(BuildDownstreamMessage(
      "project_id", "app_id", expected_data));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  expected_data.erase(expected_data.find("message_type"));
  EXPECT_EQ(MESSAGE_RECEIVED, last_event());
  EXPECT_EQ("app_id", last_app_id());
  EXPECT_EQ(expected_data.size(), last_message().data.size());
  EXPECT_EQ(expected_data, last_message().data);
}

TEST_F(GCMClientImplTest, DispatchDownstreamMessageSendError) {
  std::map<std::string, std::string> expected_data;
  expected_data["message_type"] = "send_error";
  expected_data["google.message_id"] = "007";
  MCSMessage message(BuildDownstreamMessage(
      "project_id", "app_id", expected_data));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  EXPECT_EQ(MESSAGE_SEND_ERROR, last_event());
  EXPECT_EQ("app_id", last_app_id());
  EXPECT_EQ("007", last_message_id());
}

TEST_F(GCMClientImplTest, DispatchDownstreamMessgaesDeleted) {
  std::map<std::string, std::string> expected_data;
  expected_data["message_type"] = "deleted_messages";
  MCSMessage message(BuildDownstreamMessage(
      "project_id", "app_id", expected_data));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  EXPECT_EQ(MESSAGES_DELETED, last_event());
  EXPECT_EQ("app_id", last_app_id());
}

TEST_F(GCMClientImplTest, SendMessage) {
  mcs_proto::DataMessageStanza stanza;
  stanza.set_ttl(500);

  GCMClient::OutgoingMessage message;
  message.id = "007";
  message.time_to_live = 500;
  message.data["key"] = "value";
  gcm_client()->Send("app_id", "project_id", message);

  EXPECT_EQ(kDataMessageStanzaTag, mcs_client()->last_message_tag());
  EXPECT_EQ("app_id", mcs_client()->last_data_message_stanza().category());
  EXPECT_EQ("project_id", mcs_client()->last_data_message_stanza().to());
  EXPECT_EQ(500, mcs_client()->last_data_message_stanza().ttl());
  EXPECT_EQ(CurrentTime(), mcs_client()->last_data_message_stanza().sent());
  EXPECT_EQ("007", mcs_client()->last_data_message_stanza().id());
  EXPECT_EQ("gcm@chrome.com", mcs_client()->last_data_message_stanza().from());
  EXPECT_EQ("project_id", mcs_client()->last_data_message_stanza().to());
  EXPECT_EQ("key", mcs_client()->last_data_message_stanza().app_data(0).key());
  EXPECT_EQ("value",
            mcs_client()->last_data_message_stanza().app_data(0).value());
}

}  // namespace gcm
