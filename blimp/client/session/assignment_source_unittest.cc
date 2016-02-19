// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/assignment_source.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "blimp/client/app/blimp_client_switches.h"
#include "blimp/common/protocol_version.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;

namespace blimp {
namespace client {
namespace {

MATCHER_P(AssignmentEquals, assignment, "") {
  return arg.transport_protocol == assignment.transport_protocol &&
         arg.ip_endpoint == assignment.ip_endpoint &&
         arg.client_token == assignment.client_token &&
         arg.certificate == assignment.certificate &&
         arg.certificate_fingerprint == assignment.certificate_fingerprint;
}

net::IPEndPoint BuildIPEndPoint(const std::string& ip, int port) {
  net::IPAddress ip_address;
  EXPECT_TRUE(ip_address.AssignFromIPLiteral(ip));

  return net::IPEndPoint(ip_address, port);
}

Assignment BuildValidAssignment() {
  Assignment assignment;
  assignment.transport_protocol = Assignment::TransportProtocol::SSL;
  assignment.ip_endpoint = BuildIPEndPoint("100.150.200.250", 500);
  assignment.client_token = "SecretT0kenz";
  assignment.certificate_fingerprint = "WhaleWhaleWhale";
  assignment.certificate = "whaaaaaaaaaaaaale";
  return assignment;
}

std::string BuildResponseFromAssignment(const Assignment& assignment) {
  base::DictionaryValue dict;
  dict.SetString("clientToken", assignment.client_token);
  dict.SetString("host", assignment.ip_endpoint.address().ToString());
  dict.SetInteger("port", assignment.ip_endpoint.port());
  dict.SetString("certificateFingerprint", assignment.certificate_fingerprint);
  dict.SetString("certificate", assignment.certificate);

  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

class AssignmentSourceTest : public testing::Test {
 public:
  AssignmentSourceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_),
        source_(task_runner_, task_runner_) {}

  // This expects the AssignmentSource::GetAssignment to return a custom
  // endpoint without having to hit the network.  This will typically be used
  // for testing that specifying an assignment via the command line works as
  // expected.
  void GetAlternateAssignment() {
    source_.GetAssignment("",
                          base::Bind(&AssignmentSourceTest::AssignmentResponse,
                                     base::Unretained(this)));
    EXPECT_EQ(nullptr, factory_.GetFetcherByID(0));
    task_runner_->RunUntilIdle();
  }

  // See net/base/net_errors.h for possible status errors.
  void GetNetworkAssignmentAndWaitForResponse(
      net::HttpStatusCode response_code,
      int status,
      const std::string& response,
      const std::string& client_auth_token,
      const std::string& protocol_version) {
    source_.GetAssignment(client_auth_token,
                          base::Bind(&AssignmentSourceTest::AssignmentResponse,
                                     base::Unretained(this)));

    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);

    task_runner_->RunUntilIdle();

    EXPECT_NE(nullptr, fetcher);
    EXPECT_EQ(kDefaultAssignerURL, fetcher->GetOriginalURL().spec());

    // Check that the request has a valid protocol_version.
    scoped_ptr<base::Value> json =
        base::JSONReader::Read(fetcher->upload_data());
    EXPECT_NE(nullptr, json.get());

    const base::DictionaryValue* dict;
    EXPECT_TRUE(json->GetAsDictionary(&dict));

    std::string uploaded_protocol_version;
    EXPECT_TRUE(
        dict->GetString("protocol_version", &uploaded_protocol_version));
    EXPECT_EQ(protocol_version, uploaded_protocol_version);

    // Check that the request has a valid authentication header.
    net::HttpRequestHeaders headers;
    fetcher->GetExtraRequestHeaders(&headers);

    std::string authorization;
    EXPECT_TRUE(headers.GetHeader("Authorization", &authorization));
    EXPECT_EQ("Bearer " + client_auth_token, authorization);

    // Send the fake response back.
    fetcher->set_response_code(response_code);
    fetcher->set_status(net::URLRequestStatus::FromError(status));
    fetcher->SetResponseString(response);
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    task_runner_->RunUntilIdle();
  }

  MOCK_METHOD2(AssignmentResponse,
               void(AssignmentSource::Result, const Assignment&));

 protected:
  // Used to drive all AssignmentSource tasks.
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  net::TestURLFetcherFactory factory_;

  AssignmentSource source_;
};

TEST_F(AssignmentSourceTest, TestTCPAlternateEndpointSuccess) {
  Assignment assignment;
  assignment.transport_protocol = Assignment::TransportProtocol::TCP;
  assignment.ip_endpoint = BuildIPEndPoint("100.150.200.250", 500);
  assignment.client_token = kDummyClientToken;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kBlimpletEndpoint, "tcp:100.150.200.250:500");

  EXPECT_CALL(*this, AssignmentResponse(AssignmentSource::Result::RESULT_OK,
                                        AssignmentEquals(assignment)))
      .Times(1);

  GetAlternateAssignment();
}

TEST_F(AssignmentSourceTest, TestSSLAlternateEndpointSuccess) {
  Assignment assignment;
  assignment.transport_protocol = Assignment::TransportProtocol::SSL;
  assignment.ip_endpoint = BuildIPEndPoint("100.150.200.250", 500);
  assignment.client_token = kDummyClientToken;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kBlimpletEndpoint, "ssl:100.150.200.250:500");

  EXPECT_CALL(*this, AssignmentResponse(AssignmentSource::Result::RESULT_OK,
                                        AssignmentEquals(assignment)))
      .Times(1);

  GetAlternateAssignment();
}

TEST_F(AssignmentSourceTest, TestQUICAlternateEndpointSuccess) {
  Assignment assignment;
  assignment.transport_protocol = Assignment::TransportProtocol::QUIC;
  assignment.ip_endpoint = BuildIPEndPoint("100.150.200.250", 500);
  assignment.client_token = kDummyClientToken;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kBlimpletEndpoint, "quic:100.150.200.250:500");

  EXPECT_CALL(*this, AssignmentResponse(AssignmentSource::Result::RESULT_OK,
                                        AssignmentEquals(assignment)))
      .Times(1);

  GetAlternateAssignment();
}

TEST_F(AssignmentSourceTest, TestSuccess) {
  Assignment assignment = BuildValidAssignment();

  EXPECT_CALL(*this, AssignmentResponse(AssignmentSource::Result::RESULT_OK,
                                        AssignmentEquals(assignment)))
      .Times(1);

  GetNetworkAssignmentAndWaitForResponse(
      net::HTTP_OK, net::Error::OK, BuildResponseFromAssignment(assignment),
      "UserAuthT0kenz", kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestSecondRequestInterruptsFirst) {
  InSequence sequence;
  Assignment assignment = BuildValidAssignment();

  source_.GetAssignment("",
                        base::Bind(&AssignmentSourceTest::AssignmentResponse,
                                   base::Unretained(this)));

  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_SERVER_INTERRUPTED,
                         AssignmentEquals(Assignment())))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*this, AssignmentResponse(AssignmentSource::Result::RESULT_OK,
                                        AssignmentEquals(assignment)))
      .Times(1)
      .RetiresOnSaturation();

  GetNetworkAssignmentAndWaitForResponse(
      net::HTTP_OK, net::Error::OK, BuildResponseFromAssignment(assignment),
      "UserAuthT0kenz", kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestValidAfterError) {
  InSequence sequence;
  Assignment assignment = BuildValidAssignment();

  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_NETWORK_FAILURE, _))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*this, AssignmentResponse(AssignmentSource::Result::RESULT_OK,
                                        AssignmentEquals(assignment)))
      .Times(1)
      .RetiresOnSaturation();

  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK,
                                         net::Error::ERR_INSUFFICIENT_RESOURCES,
                                         "", "UserAuthT0kenz", kEngineVersion);

  GetNetworkAssignmentAndWaitForResponse(
      net::HTTP_OK, net::Error::OK, BuildResponseFromAssignment(assignment),
      "UserAuthT0kenz", kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestNetworkFailure) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_NETWORK_FAILURE, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK,
                                         net::Error::ERR_INSUFFICIENT_RESOURCES,
                                         "", "UserAuthT0kenz", kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestBadRequest) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_BAD_REQUEST, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_BAD_REQUEST, net::Error::OK,
                                         "", "UserAuthT0kenz", kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestUnauthorized) {
  EXPECT_CALL(*this,
              AssignmentResponse(
                  AssignmentSource::Result::RESULT_EXPIRED_ACCESS_TOKEN, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_UNAUTHORIZED, net::Error::OK,
                                         "", "UserAuthT0kenz", kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestForbidden) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_USER_INVALID, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_FORBIDDEN, net::Error::OK,
                                         "", "UserAuthT0kenz", kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestTooManyRequests) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_OUT_OF_VMS, _));
  GetNetworkAssignmentAndWaitForResponse(static_cast<net::HttpStatusCode>(429),
                                         net::Error::OK, "", "UserAuthT0kenz",
                                         kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestInternalServerError) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_SERVER_ERROR, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                                         net::Error::OK, "", "UserAuthT0kenz",
                                         kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestUnexpectedNetCodeFallback) {
  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_BAD_RESPONSE, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_NOT_IMPLEMENTED,
                                         net::Error::OK, "", "UserAuthT0kenz",
                                         kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestInvalidJsonResponse) {
  Assignment assignment = BuildValidAssignment();

  // Remove half the response.
  std::string response = BuildResponseFromAssignment(assignment);
  response = response.substr(response.size() / 2);

  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_BAD_RESPONSE, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK, net::Error::OK, response,
                                         "UserAuthT0kenz", kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestMissingResponsePort) {
  // Purposely do not add the 'port' field to the response.
  base::DictionaryValue dict;
  dict.SetString("clientToken", "SecretT0kenz");
  dict.SetString("host", "happywhales");
  dict.SetString("certificateFingerprint", "WhaleWhaleWhale");
  dict.SetString("certificate", "whaaaaaaaaaaaaale");

  std::string response;
  base::JSONWriter::Write(dict, &response);

  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_BAD_RESPONSE, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK, net::Error::OK, response,
                                         "UserAuthT0kenz", kEngineVersion);
}

TEST_F(AssignmentSourceTest, TestInvalidIPAddress) {
  // Purposely add an invalid IP field to the response.
  base::DictionaryValue dict;
  dict.SetString("clientToken", "SecretT0kenz");
  dict.SetString("host", "happywhales");
  dict.SetInteger("port", 500);
  dict.SetString("certificateFingerprint", "WhaleWhaleWhale");
  dict.SetString("certificate", "whaaaaaaaaaaaaale");

  std::string response;
  base::JSONWriter::Write(dict, &response);

  EXPECT_CALL(*this, AssignmentResponse(
                         AssignmentSource::Result::RESULT_BAD_RESPONSE, _));
  GetNetworkAssignmentAndWaitForResponse(net::HTTP_OK, net::Error::OK, response,
                                         "UserAuthT0kenz", kEngineVersion);
}

}  // namespace
}  // namespace client
}  // namespace blimp
