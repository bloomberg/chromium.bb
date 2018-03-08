// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/merkle_integrity_source_stream.h"
#include "base/base64url.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/mock_source_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kBigBufferSize = 4096;
const int kSmallBufferSize = 1;

const char kMIEmptyBody[] =
    "mi-sha256=bjQLnP-zepicpUTmu3gKLHiQHT-zNzh2hRGjBhevoB0";
const char kMISingleRecord[] =
    "mi-sha256=dcRDgR2GM35DluAV13PzgnG6-pvQwPywfFvAu1UeFrs";
const char kMIMultipleRecords[] =
    "mi-sha256=IVa9shfs0nyKEhHqtB3WVNANJ2Njm5KjQLjRtnbkYJ4";

enum class ReadResultType {
  // Each call to AddReadResult is a separate read from the lower layer
  // SourceStream.
  EVERYTHING_AT_ONCE,
  // Whenever AddReadResult is called, each byte is actually a separate read
  // result.
  ONE_BYTE_AT_A_TIME,
};

struct MerkleIntegrityTestParam {
  MerkleIntegrityTestParam(int buf_size,
                           net::MockSourceStream::Mode read_mode,
                           ReadResultType read_result_type)
      : buffer_size(buf_size),
        mode(read_mode),
        read_result_type(read_result_type) {}

  const int buffer_size;
  const net::MockSourceStream::Mode mode;
  const ReadResultType read_result_type;
};

}  // namespace

class MerkleIntegritySourceStreamTest
    : public ::testing::TestWithParam<MerkleIntegrityTestParam> {
 protected:
  MerkleIntegritySourceStreamTest()
      : output_buffer_size_(GetParam().buffer_size) {}

  void Init(const std::string& mi_header_value) {
    output_buffer_ = new net::IOBuffer(output_buffer_size_);
    std::unique_ptr<net::MockSourceStream> source(new net::MockSourceStream());
    if (GetParam().read_result_type == ReadResultType::ONE_BYTE_AT_A_TIME)
      source->set_read_one_byte_at_a_time(true);
    source_ = source.get();
    stream_ = std::make_unique<MerkleIntegritySourceStream>(mi_header_value,
                                                            std::move(source));
  }

  // If MockSourceStream::Mode is ASYNC, completes reads from |mock_stream|
  // until there's no pending read, and then returns |callback|'s result, once
  // it's invoked. If Mode is not ASYNC, does nothing and returns
  // |previous_result|.
  int CompleteReadsIfAsync(int previous_result,
                           net::TestCompletionCallback* callback,
                           net::MockSourceStream* mock_stream) {
    if (GetParam().mode == net::MockSourceStream::ASYNC) {
      EXPECT_EQ(net::ERR_IO_PENDING, previous_result);
      while (mock_stream->awaiting_completion())
        mock_stream->CompleteNextRead();
      return callback->WaitForResult();
    }
    return previous_result;
  }

  net::IOBuffer* output_buffer() { return output_buffer_.get(); }
  char* output_data() { return output_buffer_->data(); }
  size_t output_buffer_size() { return output_buffer_size_; }

  net::MockSourceStream* source() { return source_; }
  MerkleIntegritySourceStream* stream() { return stream_.get(); }

  // Reads from |stream_| until an error occurs or the EOF is reached.
  // When an error occurs, returns the net error code. When an EOF is reached,
  // returns the number of bytes read and appends data read to |output|.
  int ReadStream(std::string* output) {
    int bytes_read = 0;
    while (true) {
      net::TestCompletionCallback callback;
      int rv = stream_->Read(output_buffer(), output_buffer_size(),
                             callback.callback());
      if (rv == net::ERR_IO_PENDING)
        rv = CompleteReadsIfAsync(rv, &callback, source());
      if (rv == net::OK)
        break;
      if (rv < net::OK)
        return rv;
      EXPECT_GT(rv, net::OK);
      bytes_read += rv;
      output->append(output_data(), rv);
    }
    return bytes_read;
  }

  std::string Base64Decode(const char* hash) {
    std::string out;
    EXPECT_TRUE(base::Base64UrlDecode(
        hash, base::Base64UrlDecodePolicy::DISALLOW_PADDING, &out));
    EXPECT_EQ(32u, out.size());
    return out;
  }

 private:
  scoped_refptr<net::IOBuffer> output_buffer_;
  int output_buffer_size_;

  net::MockSourceStream* source_;
  std::unique_ptr<MerkleIntegritySourceStream> stream_;
};

INSTANTIATE_TEST_CASE_P(
    MerkleIntegritySourceStreamTests,
    MerkleIntegritySourceStreamTest,
    ::testing::Values(
        MerkleIntegrityTestParam(kBigBufferSize,
                                 net::MockSourceStream::SYNC,
                                 ReadResultType::EVERYTHING_AT_ONCE),
        MerkleIntegrityTestParam(kSmallBufferSize,
                                 net::MockSourceStream::SYNC,
                                 ReadResultType::EVERYTHING_AT_ONCE),
        MerkleIntegrityTestParam(kBigBufferSize,
                                 net::MockSourceStream::ASYNC,
                                 ReadResultType::EVERYTHING_AT_ONCE),
        MerkleIntegrityTestParam(kSmallBufferSize,
                                 net::MockSourceStream::ASYNC,
                                 ReadResultType::EVERYTHING_AT_ONCE),
        MerkleIntegrityTestParam(kBigBufferSize,
                                 net::MockSourceStream::SYNC,
                                 ReadResultType::ONE_BYTE_AT_A_TIME),
        MerkleIntegrityTestParam(kSmallBufferSize,
                                 net::MockSourceStream::SYNC,
                                 ReadResultType::ONE_BYTE_AT_A_TIME),
        MerkleIntegrityTestParam(kBigBufferSize,
                                 net::MockSourceStream::ASYNC,
                                 ReadResultType::ONE_BYTE_AT_A_TIME),
        MerkleIntegrityTestParam(kSmallBufferSize,
                                 net::MockSourceStream::ASYNC,
                                 ReadResultType::ONE_BYTE_AT_A_TIME)));

TEST_P(MerkleIntegritySourceStreamTest, EmptyStream) {
  Init(kMIEmptyBody);
  source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);
  std::string actual_output;
  int result = ReadStream(&actual_output);
  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED, result);
}

TEST_P(MerkleIntegritySourceStreamTest, TooShortMIHeader) {
  Init("z");
  source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);
  std::string actual_output;
  int result = ReadStream(&actual_output);
  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED, result);
}

TEST_P(MerkleIntegritySourceStreamTest, MalformedMIHeader) {
  Init("invalid-MI-header-value");
  source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);
  std::string actual_output;
  int result = ReadStream(&actual_output);
  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED, result);
}

TEST_P(MerkleIntegritySourceStreamTest, RecordSizeOnly) {
  Init(kMIEmptyBody);
  const uint8_t record_size[] = {0, 0, 0, 0, 0, 0, 0, 10};
  source()->AddReadResult(reinterpret_cast<const char*>(record_size),
                          sizeof(record_size), net::OK, GetParam().mode);
  source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);
  std::string actual_output;
  int result = ReadStream(&actual_output);
  EXPECT_EQ(net::OK, result);
}

TEST_P(MerkleIntegritySourceStreamTest, RecordSizeOnlyWrongHash) {
  Init(kMISingleRecord);
  const uint8_t record_size[] = {0, 0, 0, 0, 0, 0, 0, 10};
  source()->AddReadResult(reinterpret_cast<const char*>(record_size),
                          sizeof(record_size), net::OK, GetParam().mode);
  source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);
  std::string actual_output;
  int result = ReadStream(&actual_output);
  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED, result);
}

TEST_P(MerkleIntegritySourceStreamTest, RecordSizeHuge) {
  Init(kMIEmptyBody);
  // 2^64 - 1 is far too large.
  const uint8_t record_size[] = {0xff, 0xff, 0xff, 0xff,
                                 0xff, 0xff, 0xff, 0xff};
  source()->AddReadResult(reinterpret_cast<const char*>(record_size),
                          sizeof(record_size), net::OK, GetParam().mode);
  std::string actual_output;
  int result = ReadStream(&actual_output);
  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED, result);
}

TEST_P(MerkleIntegritySourceStreamTest, RecordSizeTooBig) {
  Init(kMIEmptyBody);
  // 2^16 + 1 just exceeds the limit.
  const uint8_t record_size[] = {0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x40, 0x01};
  source()->AddReadResult(reinterpret_cast<const char*>(record_size),
                          sizeof(record_size), net::OK, GetParam().mode);
  std::string actual_output;
  int result = ReadStream(&actual_output);
  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED, result);
}

// https://tools.ietf.org/html/draft-thomson-http-mice-02#section-4.1
TEST_P(MerkleIntegritySourceStreamTest, SingleRecord) {
  Init(kMISingleRecord);
  const uint8_t record_size[] = {0, 0, 0, 0, 0, 0, 0, 0x29};
  const std::string message("When I grow up, I want to be a watermelon");
  source()->AddReadResult(reinterpret_cast<const char*>(record_size),
                          sizeof(record_size), net::OK, GetParam().mode);
  source()->AddReadResult(message.data(), message.size(), net::OK,
                          GetParam().mode);
  source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(message.size()), rv);
  EXPECT_EQ(message, actual_output);
}

TEST_P(MerkleIntegritySourceStreamTest, SingleRecordWrongHash) {
  Init(kMIEmptyBody);
  const uint8_t record_size[] = {0, 0, 0, 0, 0, 0, 0, 0x29};
  const std::string message("When I grow up, I want to be a watermelon");
  source()->AddReadResult(reinterpret_cast<const char*>(record_size),
                          sizeof(record_size), net::OK, GetParam().mode);
  source()->AddReadResult(message.data(), message.size(), net::OK,
                          GetParam().mode);
  source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED, rv);
  EXPECT_EQ(0u, actual_output.size());
}

// https://tools.ietf.org/html/draft-thomson-http-mice-02#section-4.2
TEST_P(MerkleIntegritySourceStreamTest, MultipleRecords) {
  Init(kMIMultipleRecords);
  const uint8_t record_size[] = {0, 0, 0, 0, 0, 0, 0, 16};
  const std::string message("When I grow up, I want to be a watermelon");

  source()->AddReadResult(reinterpret_cast<const char*>(record_size),
                          sizeof(record_size), net::OK, GetParam().mode);
  source()->AddReadResult(message.data(), 16, net::OK, GetParam().mode);
  std::string hash1 =
      Base64Decode("OElbplJlPK-Rv6JNK6p5_515IaoPoZo-2elWL7OQ60A");
  source()->AddReadResult(hash1.data(), 32, net::OK, GetParam().mode);
  source()->AddReadResult(message.data() + 16, 16, net::OK, GetParam().mode);
  std::string hash2 =
      Base64Decode("iPMpmgExHPrbEX3_RvwP4d16fWlK4l--p75PUu_KyN0");
  source()->AddReadResult(hash2.data(), 32, net::OK, GetParam().mode);
  source()->AddReadResult(message.data() + 32, message.size() - 32, net::OK,
                          GetParam().mode);

  source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(message.size()), rv);
  EXPECT_EQ(message, actual_output);
}

TEST_P(MerkleIntegritySourceStreamTest, MultipleRecordsWrongLastRecordHash) {
  Init(kMIMultipleRecords);
  const uint8_t record_size[] = {0, 0, 0, 0, 0, 0, 0, 16};
  const std::string message("When I grow up, I want to be a watermelon!");

  source()->AddReadResult(reinterpret_cast<const char*>(record_size),
                          sizeof(record_size), net::OK, GetParam().mode);
  source()->AddReadResult(message.data(), 16, net::OK, GetParam().mode);
  std::string hash1 =
      Base64Decode("OElbplJlPK-Rv6JNK6p5_515IaoPoZo-2elWL7OQ60A");
  source()->AddReadResult(hash1.data(), 32, net::OK, GetParam().mode);
  source()->AddReadResult(message.data() + 16, 16, net::OK, GetParam().mode);
  std::string hash2 =
      Base64Decode("iPMpmgExHPrbEX3_RvwP4d16fWlK4l--p75PUu_KyN0");
  source()->AddReadResult(hash2.data(), 32, net::OK, GetParam().mode);
  source()->AddReadResult(message.data() + 32, message.size() - 32, net::OK,
                          GetParam().mode);
  source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);

  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED, rv);
  EXPECT_EQ(message.substr(0, 32), actual_output);
}

}  // namespace content
