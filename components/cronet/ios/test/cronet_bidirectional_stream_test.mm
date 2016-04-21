// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include <stdint.h>
#include <list>
#include <map>
#include <string>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "components/cronet/ios/cronet_c_for_grpc.h"
#include "components/cronet/ios/cronet_environment.h"
#include "components/cronet/ios/test/quic_test_server.h"
#include "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/mock_cert_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace {
const char kTestServerHost[] = "test.example.com";
const int kTestServerPort = 6121;
const char kTestServerUrl[] = "https://test.example.com:6121";

cronet_bidirectional_stream_header kTestHeaders[] = {
    {"header1", "foo"},
    {"header2", "bar"},
};
const cronet_bidirectional_stream_header_array kTestHeadersArray = {
    2, 2, kTestHeaders};
}  // namespace

class CronetBidirectionalStreamTest : public ::testing::Test {
 protected:
  CronetBidirectionalStreamTest() {}
  ~CronetBidirectionalStreamTest() override {}

  void SetUp() override {
    static bool initialized = false;
    if (!initialized) {
      initialized = true;
      // Hack to work around issues with SetUp being called multiple times
      // during the test, and QuicTestServer not shutting down / restarting
      // gracefully.
      cronet::CronetEnvironment::Initialize();
      cronet::StartQuicTestServer();
    }

    cronet_environment_ = new cronet::CronetEnvironment("CronetTest/1.0.0.0");
    cronet_environment_->set_http2_enabled(true);
    cronet_environment_->set_quic_enabled(true);
    cronet_environment_->set_ssl_key_log_file_name("SSLKEYLOGFILE");

    std::unique_ptr<net::MockCertVerifier> mock_cert_verifier(
        new net::MockCertVerifier());
    mock_cert_verifier->set_default_result(net::OK);

    cronet_environment_->set_cert_verifier(std::move(mock_cert_verifier));
    cronet_environment_->set_host_resolver_rules(
        "MAP test.example.com 127.0.0.1,"
        "MAP notfound.example.com ~NOTFOUND");
    cronet_environment_->AddQuicHint(kTestServerHost, kTestServerPort,
                                     kTestServerPort);

    cronet_environment_->Start();

    cronet_engine_.obj = cronet_environment_;

    cronet_environment_->StartNetLog("cronet_netlog.json", true);
  }

  void TearDown() override {
    // cronet::ShutdownQuicTestServer();
    cronet_environment_->StopNetLog();
    //[CronetEngine stopNetLog];
    //[CronetEngine uninstall];
  }

  cronet_engine* engine() { return &cronet_engine_; }

 private:
  static cronet::CronetEnvironment* cronet_environment_;
  static cronet_engine cronet_engine_;
};

cronet::CronetEnvironment* CronetBidirectionalStreamTest::cronet_environment_;
cronet_engine CronetBidirectionalStreamTest::cronet_engine_;

class TestBidirectionalStreamCallback {
 public:
  enum ResponseStep {
    NOTHING,
    ON_REQUEST_HEADERS_SENT,
    ON_RESPONSE_STARTED,
    ON_READ_COMPLETED,
    ON_WRITE_COMPLETED,
    ON_TRAILERS,
    ON_CANCELED,
    ON_FAILED,
    ON_SUCCEEDED
  };

  cronet_bidirectional_stream* stream;
  base::WaitableEvent stream_done_event;

  // Test parameters.
  std::map<std::string, std::string> request_headers;
  std::list<std::string> write_data;
  std::string expected_negotiated_protocol;
  ResponseStep cancel_from_step;
  size_t read_buffer_size;

  // Test results.
  ResponseStep response_step;
  char* read_buffer;
  std::map<std::string, std::string> response_headers;
  std::map<std::string, std::string> response_trailers;
  std::vector<std::string> read_data;
  int net_error;

  TestBidirectionalStreamCallback()
      : stream(nullptr),
        stream_done_event(true, false),
        expected_negotiated_protocol("quic/1+spdy/3"),
        cancel_from_step(NOTHING),
        read_buffer_size(32768),
        response_step(NOTHING),
        read_buffer(nullptr),
        net_error(0) {}

  ~TestBidirectionalStreamCallback() {
    if (read_buffer)
      delete read_buffer;
  }

  static TestBidirectionalStreamCallback* FromStream(
      cronet_bidirectional_stream* stream) {
    DCHECK(stream);
    return (TestBidirectionalStreamCallback*)stream->annotation;
  }

  bool MaybeCancel(cronet_bidirectional_stream* stream, ResponseStep step) {
    DCHECK_EQ(stream, this->stream);
    response_step = step;
    DLOG(WARNING) << "Step: " << step;

    if (step != cancel_from_step)
      return false;

    cronet_bidirectional_stream_cancel(stream);
    return true;
  }

  void SignalDone() { stream_done_event.Signal(); }

  void BlockForDone() { stream_done_event.Wait(); }

  void AddWriteData(const std::string& data) { write_data.push_back(data); }

  void MaybeWriteNextData(cronet_bidirectional_stream* stream) {
    DCHECK_EQ(stream, this->stream);
    if (write_data.empty())
      return;
    cronet_bidirectional_stream_write(stream, write_data.front().c_str(),
                                      write_data.front().size(),
                                      write_data.size() == 1);
  }

  cronet_bidirectional_stream_callback* callback() const { return &s_callback; }

 private:
  // C callbacks.
  static void on_request_headers_sent_callback(
      cronet_bidirectional_stream* stream) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    if (test->MaybeCancel(stream, ON_REQUEST_HEADERS_SENT))
      return;
    test->MaybeWriteNextData(stream);
  }

  static void on_response_headers_received_callback(
      cronet_bidirectional_stream* stream,
      const cronet_bidirectional_stream_header_array* headers,
      const char* negotiated_protocol) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    ASSERT_EQ(test->expected_negotiated_protocol,
              std::string(negotiated_protocol));
    for (size_t i = 0; i < headers->count; ++i) {
      test->response_headers[headers->headers[i].key] =
          headers->headers[i].value;
    }
    if (test->MaybeCancel(stream, ON_RESPONSE_STARTED))
      return;
    test->read_buffer = new char[test->read_buffer_size];
    cronet_bidirectional_stream_read(stream, test->read_buffer,
                                     test->read_buffer_size);
  }

  static void on_read_completed_callback(cronet_bidirectional_stream* stream,
                                         char* data,
                                         int count) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    test->read_data.push_back(std::string(data, count));
    if (test->MaybeCancel(stream, ON_READ_COMPLETED))
      return;
    if (count == 0)
      return;
    cronet_bidirectional_stream_read(stream, test->read_buffer,
                                     test->read_buffer_size);
  }

  static void on_write_completed_callback(cronet_bidirectional_stream* stream,
                                          const char* data) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    ASSERT_EQ(test->write_data.front().c_str(), data);
    if (test->MaybeCancel(stream, ON_WRITE_COMPLETED))
      return;
    test->write_data.pop_front();
    test->MaybeWriteNextData(stream);
  }

  static void on_response_trailers_received_callback(
      cronet_bidirectional_stream* stream,
      const cronet_bidirectional_stream_header_array* trailers) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    if (test->MaybeCancel(stream, ON_TRAILERS))
      return;
  }

  static void on_succeded_callback(cronet_bidirectional_stream* stream) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    test->MaybeCancel(stream, ON_SUCCEEDED);
    test->SignalDone();
  }

  static void on_failed_callback(cronet_bidirectional_stream* stream,
                                 int net_error) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    test->net_error = net_error;
    test->MaybeCancel(stream, ON_FAILED);
    test->SignalDone();
  }

  static void on_canceled_callback(cronet_bidirectional_stream* stream) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    test->MaybeCancel(stream, ON_CANCELED);
    test->SignalDone();
  }

  static cronet_bidirectional_stream_callback s_callback;
};

cronet_bidirectional_stream_callback
    TestBidirectionalStreamCallback::s_callback = {
        on_request_headers_sent_callback,
        on_response_headers_received_callback,
        on_read_completed_callback,
        on_write_completed_callback,
        on_response_trailers_received_callback,
        on_succeded_callback,
        on_failed_callback,
        on_canceled_callback};

TEST_F(CronetBidirectionalStreamTest, StartExampleBidiStream) {
  TestBidirectionalStreamCallback test;
  test.AddWriteData("Hello, ");
  test.AddWriteData("world!");
  test.read_buffer_size = 2;
  test.stream =
      cronet_bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  cronet_bidirectional_stream_start(test.stream, kTestServerUrl, 0, "POST",
                                    &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(std::string("404"), test.response_headers[":status"]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(std::string("fi"), test.read_data.front());
  ASSERT_EQ(std::string("file not found"),
            base::JoinString(test.read_data, ""));
  cronet_bidirectional_stream_destroy(test.stream);
}

TEST_F(CronetBidirectionalStreamTest, CancelOnRead) {
  TestBidirectionalStreamCallback test;
  test.stream =
      cronet_bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_READ_COMPLETED;
  cronet_bidirectional_stream_start(test.stream, kTestServerUrl, 0, "POST",
                                    &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_EQ(std::string("404"), test.response_headers[":status"]);
  ASSERT_EQ(std::string("file not found"), test.read_data.front());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_CANCELED, test.response_step);
  cronet_bidirectional_stream_destroy(test.stream);
}

TEST_F(CronetBidirectionalStreamTest, CancelOnResponse) {
  TestBidirectionalStreamCallback test;
  test.stream =
      cronet_bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_RESPONSE_STARTED;
  cronet_bidirectional_stream_start(test.stream, kTestServerUrl, 0, "POST",
                                    &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_EQ(std::string("404"), test.response_headers[":status"]);
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_CANCELED, test.response_step);
  cronet_bidirectional_stream_destroy(test.stream);
}

TEST_F(CronetBidirectionalStreamTest, CancelOnSucceeded) {
  TestBidirectionalStreamCallback test;
  test.stream =
      cronet_bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_SUCCEEDED;
  cronet_bidirectional_stream_start(test.stream, kTestServerUrl, 0, "POST",
                                    &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_EQ(std::string("404"), test.response_headers[":status"]);
  ASSERT_EQ(std::string("file not found"), test.read_data.front());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  cronet_bidirectional_stream_destroy(test.stream);
}

TEST_F(CronetBidirectionalStreamTest, ReadFailsBeforeRequestStarted) {
  TestBidirectionalStreamCallback test;
  test.stream =
      cronet_bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  char read_buffer[1];
  cronet_bidirectional_stream_read(test.stream, read_buffer,
                                   sizeof(read_buffer));
  test.BlockForDone();
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_EQ(net::ERR_UNEXPECTED, test.net_error);
  cronet_bidirectional_stream_destroy(test.stream);
}

TEST_F(CronetBidirectionalStreamTest, WriteFailsBeforeRequestStarted) {
  TestBidirectionalStreamCallback test;
  test.stream =
      cronet_bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  cronet_bidirectional_stream_write(test.stream, "1", 1, false);
  test.BlockForDone();
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_EQ(net::ERR_UNEXPECTED, test.net_error);
  cronet_bidirectional_stream_destroy(test.stream);
}

TEST_F(CronetBidirectionalStreamTest, FailedResolution) {
  TestBidirectionalStreamCallback test;
  test.stream =
      cronet_bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_FAILED;
  cronet_bidirectional_stream_start(test.stream, "https://notfound.example.com",
                                    0, "GET", &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_EQ(net::ERR_NAME_NOT_RESOLVED, test.net_error);
  cronet_bidirectional_stream_destroy(test.stream);
}
