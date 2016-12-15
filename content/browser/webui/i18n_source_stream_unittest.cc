// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/browser/webui/i18n_source_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/mock_source_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// These constants are rather arbitrary, though the offsets and other sizes must
// be less than kBufferSize.
const int kBufferSize = 256;
const int kSmallBufferSize = 1;

const int kShortReplacementOffset = 5;
const char kShortReplacementKey[] = "a";
const char kShortReplacementToken[] = "$i18n{a}";
const char kShortReplacementValue[] = "short";

const int kLongReplacementOffset = 33;
const char kLongReplacementKey[] = "aLongerReplacementName";
const char kLongReplacementToken[] = "$i18n{aLongerReplacementName}";
const char kLongReplacementValue[] = "second replacement";

const int kSourceSize =
    50 + arraysize(kShortReplacementToken) + arraysize(kLongReplacementToken);
const int kResultSize =
    50 + arraysize(kShortReplacementValue) + arraysize(kLongReplacementValue);

struct I18nTestParam {
  I18nTestParam(int buf_size, net::MockSourceStream::Mode read_mode)
      : buffer_size(buf_size), mode(read_mode) {}

  const int buffer_size;
  const net::MockSourceStream::Mode mode;
};

}  // namespace

class I18nSourceStreamTest : public ::testing::TestWithParam<I18nTestParam> {
 protected:
  I18nSourceStreamTest() : output_buffer_size_(GetParam().buffer_size) {}

  // Helpful function to initialize the test fixture.
  void Init() {
    source_data_len_ = kBufferSize;
    for (size_t i = 0; i < source_data_len_; i++)
      source_data_[i] = i % 256;

    // Inserts must be done last to first as they appear in the buffer.
    InsertText(source_data_, source_data_len_, kLongReplacementOffset,
               kLongReplacementToken);
    InsertText(source_data_, source_data_len_, kShortReplacementOffset,
               kShortReplacementToken);

    result_data_len_ = kBufferSize;
    for (size_t i = 0; i < result_data_len_; i++)
      result_data_[i] = i % 256;

    // Inserts must be done last to first as they appear in the buffer.
    InsertText(result_data_, result_data_len_, kLongReplacementOffset,
               kLongReplacementValue);
    InsertText(result_data_, result_data_len_, kShortReplacementOffset,
               kShortReplacementValue);

    output_buffer_ = new net::IOBuffer(output_buffer_size_);
    std::unique_ptr<net::MockSourceStream> source(new net::MockSourceStream());
    source_ = source.get();

    replacements_[kShortReplacementKey] = kShortReplacementValue;
    replacements_[kLongReplacementKey] = kLongReplacementValue;
    stream_ = I18nSourceStream::Create(
        std::move(source), net::SourceStream::TYPE_NONE, &replacements_);
  }

  // If MockSourceStream::Mode is ASYNC, completes 1 read from |mock_stream| and
  // wait for |callback| to complete. If Mode is not ASYNC, does nothing and
  // returns |previous_result|.
  int CompleteReadIfAsync(int previous_result,
                          net::TestCompletionCallback* callback,
                          net::MockSourceStream* mock_stream) {
    if (GetParam().mode == net::MockSourceStream::ASYNC) {
      EXPECT_EQ(net::ERR_IO_PENDING, previous_result);
      mock_stream->CompleteNextRead();
      return callback->WaitForResult();
    }
    return previous_result;
  }

  void InsertText(char* buffer,
                  size_t buffer_length,
                  size_t offset,
                  const char* text) {
    // Intended to be dead simple so that it can be confirmed
    // as correct by hand.
    size_t text_length = strlen(text);
    memmove(buffer + offset + text_length, buffer + offset,
            buffer_length - offset - text_length);
    memcpy(buffer + offset, text, text_length);
  }

  char* source_data() { return source_data_; }
  size_t source_data_len() { return source_data_len_; }

  char* result_data() { return result_data_; }
  size_t result_data_len() { return result_data_len_; }

  net::IOBuffer* output_buffer() { return output_buffer_.get(); }
  char* output_data() { return output_buffer_->data(); }
  size_t output_buffer_size() { return output_buffer_size_; }

  net::MockSourceStream* source() { return source_; }
  I18nSourceStream* stream() { return stream_.get(); }

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
        rv = CompleteReadIfAsync(rv, &callback, source());
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

 private:
  char source_data_[kBufferSize];
  size_t source_data_len_;

  char result_data_[kBufferSize];
  size_t result_data_len_;

  scoped_refptr<net::IOBuffer> output_buffer_;
  const int output_buffer_size_;

  net::MockSourceStream* source_;
  std::unique_ptr<I18nSourceStream> stream_;

  ui::TemplateReplacements replacements_;
};

INSTANTIATE_TEST_CASE_P(
    I18nSourceStreamTests,
    I18nSourceStreamTest,
    ::testing::Values(I18nTestParam(kBufferSize, net::MockSourceStream::SYNC),
                      I18nTestParam(kSmallBufferSize,
                                    net::MockSourceStream::SYNC)));

TEST_P(I18nSourceStreamTest, EmptyStream) {
  Init();
  source()->AddReadResult("", 0, net::OK, GetParam().mode);
  std::string actual_output;
  int result = ReadStream(&actual_output);
  EXPECT_EQ(net::OK, result);
  EXPECT_EQ("i18n", stream()->Description());
}

TEST_P(I18nSourceStreamTest, NoTranslations) {
  Init();
  const char kText[] = "This text has no i18n replacements.";
  size_t kTextLength = strlen(kText);
  source()->AddReadResult(kText, kTextLength, net::OK, GetParam().mode);
  source()->AddReadResult(kText + kTextLength, 0, net::OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(kTextLength), rv);
  EXPECT_EQ(std::string(kText, kTextLength), actual_output);
  EXPECT_EQ("i18n", stream()->Description());
}

TEST_P(I18nSourceStreamTest, I18nOneRead) {
  Init();
  source()->AddReadResult(source_data(), kSourceSize, net::OK, GetParam().mode);
  source()->AddReadResult(source_data() + kSourceSize, 0, net::OK,
                          GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(kResultSize), rv);
  EXPECT_EQ(std::string(result_data(), kResultSize), actual_output);
  EXPECT_EQ("i18n", stream()->Description());
}

TEST_P(I18nSourceStreamTest, I18nInMultipleReads) {
  Init();
  size_t chunk_size = 5;
  size_t written = 0;
  while (written + chunk_size < kSourceSize) {
    source()->AddReadResult(source_data() + written, chunk_size, net::OK,
                            GetParam().mode);
    written += chunk_size;
  }
  source()->AddReadResult(source_data() + written, kSourceSize - written,
                          net::OK, GetParam().mode);
  source()->AddReadResult(source_data() + kSourceSize, 0, net::OK,
                          GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(kResultSize), rv);
  EXPECT_EQ(std::string(result_data(), kResultSize), actual_output);
  EXPECT_EQ("i18n", stream()->Description());
}

}  // namespace content
