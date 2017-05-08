// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <list>
#include <map>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "components/grpc_support/include/bidirectional_stream_c.h"
#include "components/grpc_support/test/get_stream_engine.h"
#include "components/grpc_support/test/quic_test_server.h"
#include "net/base/net_errors.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

bidirectional_stream_header kTestHeaders[] = {
    {"header1", "foo"},
    {"header2", "bar"},
};
const bidirectional_stream_header_array kTestHeadersArray = {2, 2,
                                                             kTestHeaders};
}  // namespace

namespace grpc_support {

class BidirectionalStreamTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    StartQuicTestServer();
    StartTestStreamEngine(GetQuicTestServerPort());
  }

  void TearDown() override {
    ShutdownTestStreamEngine();
    ShutdownQuicTestServer();
  }

  BidirectionalStreamTest() {}
  ~BidirectionalStreamTest() override {}

  stream_engine* engine() {
    return GetTestStreamEngine(GetQuicTestServerPort());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BidirectionalStreamTest);
};

class TestBidirectionalStreamCallback {
 public:
  enum ResponseStep {
    NOTHING,
    ON_STREAM_READY,
    ON_RESPONSE_STARTED,
    ON_READ_COMPLETED,
    ON_WRITE_COMPLETED,
    ON_TRAILERS,
    ON_CANCELED,
    ON_FAILED,
    ON_SUCCEEDED
  };

  struct WriteData {
    std::string buffer;
    // If |flush| is true, then bidirectional_stream_flush() will be
    // called after writing of the |buffer|.
    bool flush;

    WriteData(const std::string& buffer, bool flush);
    ~WriteData();

    DISALLOW_COPY_AND_ASSIGN(WriteData);
  };

  bidirectional_stream* stream;
  base::WaitableEvent stream_done_event;

  // Test parameters.
  std::map<std::string, std::string> request_headers;
  std::list<std::unique_ptr<WriteData>> write_data;
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
        stream_done_event(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED),
        expected_negotiated_protocol("quic/1+spdy/3"),
        cancel_from_step(NOTHING),
        read_buffer_size(32768),
        response_step(NOTHING),
        read_buffer(nullptr),
        net_error(0) {}

  ~TestBidirectionalStreamCallback() {
    if (read_buffer)
      delete[] read_buffer;
  }

  static TestBidirectionalStreamCallback* FromStream(
      bidirectional_stream* stream) {
    DCHECK(stream);
    return (TestBidirectionalStreamCallback*)stream->annotation;
  }

  virtual bool MaybeCancel(bidirectional_stream* stream, ResponseStep step) {
    DCHECK_EQ(stream, this->stream);
    response_step = step;
    DLOG(WARNING) << "Step: " << step;

    if (step != cancel_from_step)
      return false;

    bidirectional_stream_cancel(stream);
    bidirectional_stream_write(stream, "abc", 3, false);

    return true;
  }

  void SignalDone() { stream_done_event.Signal(); }

  void BlockForDone() { stream_done_event.Wait(); }

  void AddWriteData(const std::string& data) { AddWriteData(data, true); }
  void AddWriteData(const std::string& data, bool flush) {
    write_data.push_back(base::MakeUnique<WriteData>(data, flush));
  }

  virtual void MaybeWriteNextData(bidirectional_stream* stream) {
    DCHECK_EQ(stream, this->stream);
    if (write_data.empty())
      return;
    for (const auto& data : write_data) {
      bidirectional_stream_write(stream, data->buffer.c_str(),
                                 data->buffer.size(),
                                 data == write_data.back());
      if (data->flush) {
        bidirectional_stream_flush(stream);
        break;
      }
    }
  }

  bidirectional_stream_callback* callback() const { return &s_callback; }

 private:
  // C callbacks.
  static void on_stream_ready_callback(bidirectional_stream* stream) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    if (test->MaybeCancel(stream, ON_STREAM_READY))
      return;
    test->MaybeWriteNextData(stream);
  }

  static void on_response_headers_received_callback(
      bidirectional_stream* stream,
      const bidirectional_stream_header_array* headers,
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
    bidirectional_stream_read(stream, test->read_buffer,
                              test->read_buffer_size);
  }

  static void on_read_completed_callback(bidirectional_stream* stream,
                                         char* data,
                                         int count) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    test->read_data.push_back(std::string(data, count));
    if (test->MaybeCancel(stream, ON_READ_COMPLETED))
      return;
    if (count == 0)
      return;
    bidirectional_stream_read(stream, test->read_buffer,
                              test->read_buffer_size);
  }

  static void on_write_completed_callback(bidirectional_stream* stream,
                                          const char* data) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    ASSERT_EQ(test->write_data.front()->buffer.c_str(), data);
    if (test->MaybeCancel(stream, ON_WRITE_COMPLETED))
      return;
    bool continue_writing = test->write_data.front()->flush;
    test->write_data.pop_front();
    if (continue_writing)
      test->MaybeWriteNextData(stream);
  }

  static void on_response_trailers_received_callback(
      bidirectional_stream* stream,
      const bidirectional_stream_header_array* trailers) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    for (size_t i = 0; i < trailers->count; ++i) {
      test->response_trailers[trailers->headers[i].key] =
          trailers->headers[i].value;
    }

    if (test->MaybeCancel(stream, ON_TRAILERS))
      return;
  }

  static void on_succeded_callback(bidirectional_stream* stream) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    ASSERT_TRUE(test->write_data.empty());
    test->MaybeCancel(stream, ON_SUCCEEDED);
    test->SignalDone();
  }

  static void on_failed_callback(bidirectional_stream* stream, int net_error) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    test->net_error = net_error;
    test->MaybeCancel(stream, ON_FAILED);
    test->SignalDone();
  }

  static void on_canceled_callback(bidirectional_stream* stream) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    test->MaybeCancel(stream, ON_CANCELED);
    test->SignalDone();
  }

  static bidirectional_stream_callback s_callback;
};

bidirectional_stream_callback TestBidirectionalStreamCallback::s_callback = {
    on_stream_ready_callback,
    on_response_headers_received_callback,
    on_read_completed_callback,
    on_write_completed_callback,
    on_response_trailers_received_callback,
    on_succeded_callback,
    on_failed_callback,
    on_canceled_callback};

TestBidirectionalStreamCallback::WriteData::WriteData(const std::string& data,
                                                      bool flush_after)
    : buffer(data), flush(flush_after) {}

TestBidirectionalStreamCallback::WriteData::~WriteData() {}

TEST_P(BidirectionalStreamTest, StartExampleBidiStream) {
  TestBidirectionalStreamCallback test;
  test.AddWriteData("Hello, ");
  test.AddWriteData("world!");
  // Use small read buffer size to test that response is split properly.
  test.read_buffer_size = 2;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "POST", &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(std::string(kHelloStatus), test.response_headers[kStatusHeader]);
  ASSERT_EQ(std::string(kHelloHeaderValue),
            test.response_headers[kHelloHeaderName]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(std::string(kHelloBodyValue, 2), test.read_data.front());
  // Verify that individual read data joined using empty separator match
  // expected body.
  ASSERT_EQ(std::string(kHelloBodyValue), base::JoinString(test.read_data, ""));
  ASSERT_EQ(std::string(kHelloTrailerValue),
            test.response_trailers[kHelloTrailerName]);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, SimplePutWithEmptyWriteDataAtTheEnd) {
  TestBidirectionalStreamCallback test;
  test.AddWriteData("Hello, ");
  test.AddWriteData("world!");
  test.AddWriteData("");
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "PUT", &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(std::string(kHelloStatus), test.response_headers[kStatusHeader]);
  ASSERT_EQ(std::string(kHelloHeaderValue),
            test.response_headers[kHelloHeaderName]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(std::string(kHelloBodyValue), test.read_data.front());
  ASSERT_EQ(std::string(kHelloTrailerValue),
            test.response_trailers[kHelloTrailerName]);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, SimpleGetWithFlush) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_disable_auto_flush(test.stream, true);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  // Flush before start is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "GET", &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_EQ(std::string(kHelloStatus), test.response_headers[kStatusHeader]);
  ASSERT_EQ(std::string(kHelloHeaderValue),
            test.response_headers[kHelloHeaderName]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(std::string(kHelloBodyValue), base::JoinString(test.read_data, ""));
  ASSERT_EQ(std::string(kHelloTrailerValue),
            test.response_trailers[kHelloTrailerName]);
  // Flush after done is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, SimplePostWithFlush) {
  TestBidirectionalStreamCallback test;
  test.AddWriteData("Test String", false);
  test.AddWriteData("1234567890", false);
  test.AddWriteData("woot!", true);
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_disable_auto_flush(test.stream, true);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  // Flush before start is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "POST", &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(std::string(kHelloStatus), test.response_headers[kStatusHeader]);
  ASSERT_EQ(std::string(kHelloHeaderValue),
            test.response_headers[kHelloHeaderName]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(std::string(kHelloBodyValue), base::JoinString(test.read_data, ""));
  ASSERT_EQ(std::string(kHelloTrailerValue),
            test.response_trailers[kHelloTrailerName]);
  // Flush after done is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, SimplePostWithFlushTwice) {
  TestBidirectionalStreamCallback test;
  test.AddWriteData("Test String", false);
  test.AddWriteData("1234567890", false);
  test.AddWriteData("woot!", true);
  test.AddWriteData("Test String", false);
  test.AddWriteData("1234567890", false);
  test.AddWriteData("woot!", true);
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_disable_auto_flush(test.stream, true);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  // Flush before start is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "POST", &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(std::string(kHelloStatus), test.response_headers[kStatusHeader]);
  ASSERT_EQ(std::string(kHelloHeaderValue),
            test.response_headers[kHelloHeaderName]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(std::string(kHelloBodyValue), base::JoinString(test.read_data, ""));
  ASSERT_EQ(std::string(kHelloTrailerValue),
            test.response_trailers[kHelloTrailerName]);
  // Flush after done is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, SimplePostWithFlushAfterOneWrite) {
  TestBidirectionalStreamCallback test;
  test.AddWriteData("Test String", false);
  test.AddWriteData("1234567890", false);
  test.AddWriteData("woot!", true);
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_disable_auto_flush(test.stream, true);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  // Flush before start is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "POST", &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(std::string(kHelloStatus), test.response_headers[kStatusHeader]);
  ASSERT_EQ(std::string(kHelloHeaderValue),
            test.response_headers[kHelloHeaderName]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(std::string(kHelloBodyValue), base::JoinString(test.read_data, ""));
  ASSERT_EQ(std::string(kHelloTrailerValue),
            test.response_trailers[kHelloTrailerName]);
  // Flush after done is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, TestDelayedFlush) {
  class CustomTestBidirectionalStreamCallback
      : public TestBidirectionalStreamCallback {
    void MaybeWriteNextData(bidirectional_stream* stream) override {
      DCHECK_EQ(stream, this->stream);
      if (write_data.empty())
        return;
      // Write all buffers when stream is ready.
      // Flush after "3" and "5".
      // EndOfStream is set with "6" but not flushed, so it is not sent.
      if (write_data.front()->buffer == "1") {
        for (const auto& data : write_data) {
          bidirectional_stream_write(stream, data->buffer.c_str(),
                                     data->buffer.size(),
                                     data == write_data.back());
          if (data->flush) {
            bidirectional_stream_flush(stream);
          }
        }
      }
      // Flush the final buffer with endOfStream flag.
      if (write_data.front()->buffer == "6")
        bidirectional_stream_flush(stream);
    }
  };

  CustomTestBidirectionalStreamCallback test;
  test.AddWriteData("1", false);
  test.AddWriteData("2", false);
  test.AddWriteData("3", true);
  test.AddWriteData("4", false);
  test.AddWriteData("5", true);
  test.AddWriteData("6", false);
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_disable_auto_flush(test.stream, true);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  // Flush before start is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "POST", &kTestHeadersArray, false);
  test.BlockForDone();
  // Flush after done is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, CancelOnRead) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_READ_COMPLETED;
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "POST", &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_EQ(std::string(kHelloStatus), test.response_headers[kStatusHeader]);
  ASSERT_EQ(std::string(kHelloBodyValue), test.read_data.front());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_CANCELED, test.response_step);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, CancelOnResponse) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_RESPONSE_STARTED;
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "POST", &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_EQ(std::string(kHelloStatus), test.response_headers[kStatusHeader]);
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_CANCELED, test.response_step);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, CancelOnSucceeded) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_SUCCEEDED;
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "POST", &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_EQ(std::string(kHelloStatus), test.response_headers[kStatusHeader]);
  ASSERT_EQ(std::string(kHelloBodyValue), test.read_data.front());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, ReadFailsBeforeRequestStarted) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  char read_buffer[1];
  bidirectional_stream_read(test.stream, read_buffer, sizeof(read_buffer));
  test.BlockForDone();
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_EQ(net::ERR_UNEXPECTED, test.net_error);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, StreamFailBeforeReadIsExecutedOnNetworkThread) {
  class CustomTestBidirectionalStreamCallback
      : public TestBidirectionalStreamCallback {
    bool MaybeCancel(bidirectional_stream* stream, ResponseStep step) override {
      if (step == ResponseStep::ON_READ_COMPLETED) {
        // Shut down the server, and the stream should error out.
        // The second call to ShutdownQuicTestServer is no-op.
        ShutdownQuicTestServer();
      }
      return TestBidirectionalStreamCallback::MaybeCancel(stream, step);
    }
  };

  CustomTestBidirectionalStreamCallback test;
  test.AddWriteData("Hello, ");
  test.AddWriteData("world!");
  test.read_buffer_size = 2;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "POST", &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_TRUE(test.net_error == net::ERR_QUIC_PROTOCOL_ERROR ||
              test.net_error == net::ERR_CONNECTION_REFUSED);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, WriteFailsBeforeRequestStarted) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_write(test.stream, "1", 1, false);
  test.BlockForDone();
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_EQ(net::ERR_UNEXPECTED, test.net_error);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, StreamFailAfterStreamReadyCallback) {
  class CustomTestBidirectionalStreamCallback
      : public TestBidirectionalStreamCallback {
    bool MaybeCancel(bidirectional_stream* stream, ResponseStep step) override {
      if (step == ResponseStep::ON_STREAM_READY) {
        // Shut down the server, and the stream should error out.
        // The second call to ShutdownQuicTestServer is no-op.
        ShutdownQuicTestServer();
      }
      return TestBidirectionalStreamCallback::MaybeCancel(stream, step);
    }
  };

  CustomTestBidirectionalStreamCallback test;
  test.AddWriteData("Test String");
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "POST", &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_TRUE(test.net_error == net::ERR_QUIC_PROTOCOL_ERROR ||
              test.net_error == net::ERR_QUIC_HANDSHAKE_FAILED ||
              test.net_error == net::ERR_CONNECTION_REFUSED);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest,
       StreamFailBeforeWriteIsExecutedOnNetworkThread) {
  class CustomTestBidirectionalStreamCallback
      : public TestBidirectionalStreamCallback {
    bool MaybeCancel(bidirectional_stream* stream, ResponseStep step) override {
      if (step == ResponseStep::ON_WRITE_COMPLETED) {
        // Shut down the server, and the stream should error out.
        // The second call to ShutdownQuicTestServer is no-op.
        ShutdownQuicTestServer();
      }
      return TestBidirectionalStreamCallback::MaybeCancel(stream, step);
    }
  };

  CustomTestBidirectionalStreamCallback test;
  test.AddWriteData("Test String");
  test.AddWriteData("1234567890");
  test.AddWriteData("woot!");
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_start(test.stream, kTestServerUrl, 0,
                             "POST", &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_TRUE(test.net_error == net::ERR_QUIC_PROTOCOL_ERROR ||
              test.net_error == net::ERR_QUIC_HANDSHAKE_FAILED);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, FailedResolution) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_FAILED;
  bidirectional_stream_start(test.stream, "https://notfound.example.com", 0,
                             "GET", &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_EQ(net::ERR_NAME_NOT_RESOLVED, test.net_error);
  bidirectional_stream_destroy(test.stream);
}

INSTANTIATE_TEST_CASE_P(BidirectionalStreamDelayRequestHeadersUntilFlush,
                        BidirectionalStreamTest,
                        ::testing::Values(true, false));

}  // namespace grpc_support
