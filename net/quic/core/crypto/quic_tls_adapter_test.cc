// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/crypto/quic_tls_adapter.h"

#include <vector>

#include "net/quic/platform/api/quic_arraysize.h"
#include "net/quic/platform/api/quic_test.h"
#include "third_party/boringssl/src/include/openssl/bio.h"

using std::string;

namespace net {
namespace test {
namespace {

class TestVisitor : public QuicTlsAdapter::Visitor {
 public:
  void OnDataAvailableForBIO() override { data_available_count_++; }

  void OnDataReceivedFromBIO(const QuicStringPiece& data) override {
    received_messages_.push_back(string(data.data(), data.length()));
  }

  int data_available_count() const { return data_available_count_; }

  const std::vector<string>& received_messages() const {
    return received_messages_;
  }

 private:
  int data_available_count_ = 0;
  std::vector<string> received_messages_;
};

class QuicTlsAdapterTest : public QuicTestWithParam<Perspective> {
 public:
  QuicTlsAdapterTest() : adapter_(&visitor_) { bio_ = adapter_.bio(); }

 protected:
  TestVisitor visitor_;
  QuicTlsAdapter adapter_;
  BIO* bio_;
};

INSTANTIATE_TEST_CASE_P(Tests,
                        QuicTlsAdapterTest,
                        ::testing::Values(Perspective::IS_CLIENT,
                                          Perspective::IS_SERVER));

TEST_P(QuicTlsAdapterTest, ProcessInput) {
  string input = "abc";
  EXPECT_TRUE(adapter_.ProcessInput(input, GetParam()));
  EXPECT_EQ(1, visitor_.data_available_count());

  char buf[4];
  ASSERT_EQ(static_cast<int>(input.length()),
            BIO_read(bio_, buf, QUIC_ARRAYSIZE(buf)));
  EXPECT_EQ(input, string(buf, input.length()));
}

TEST_P(QuicTlsAdapterTest, BIORead) {
  string input1 = "abcd";
  string input2 = "efgh";

  EXPECT_TRUE(adapter_.ProcessInput(input1, GetParam()));
  EXPECT_EQ(QUIC_NO_ERROR, adapter_.error());
  EXPECT_EQ(1, visitor_.data_available_count());

  // Test that a call to BIO_read for less than what is in |adapter_|'s buffer
  // still leaves more input remaining to read.
  char buf1[3];
  ASSERT_EQ(static_cast<int>(QUIC_ARRAYSIZE(buf1)),
            BIO_read(bio_, buf1, QUIC_ARRAYSIZE(buf1)));
  EXPECT_EQ("abc", string(buf1, QUIC_ARRAYSIZE(buf1)));
  EXPECT_EQ(1u, adapter_.InputBytesRemaining());

  // Test that the bytes read by BIO_read can span input read in by
  // ProcessInput.
  EXPECT_TRUE(adapter_.ProcessInput(input2, GetParam()));
  EXPECT_EQ(QUIC_NO_ERROR, adapter_.error());
  EXPECT_EQ(2, visitor_.data_available_count());
  char buf2[5];
  ASSERT_EQ(static_cast<int>(QUIC_ARRAYSIZE(buf2)),
            BIO_read(bio_, buf2, QUIC_ARRAYSIZE(buf2)));
  EXPECT_EQ("defgh", string(buf2, QUIC_ARRAYSIZE(buf2)));
  EXPECT_EQ(0u, adapter_.InputBytesRemaining());
}

TEST_P(QuicTlsAdapterTest, BIOWrite) {
  string input = "abcde";
  // Test that just calling BIO_write does not post any messages to the Visitor.
  EXPECT_EQ(static_cast<int>(input.length()),
            BIO_write(bio_, input.data(), input.length()));
  ASSERT_EQ(0u, visitor_.received_messages().size());

  // Test that calling BIO_flush does post the message to the Visitor.
  EXPECT_EQ(1, BIO_flush(bio_));
  ASSERT_EQ(1u, visitor_.received_messages().size());
  EXPECT_EQ(input, visitor_.received_messages()[0]);

  // Test that multiple calls to BIO_write, followed by one call to BIO_flush
  // results in only one call to Visitor::OnDataReceivedFromBIO.
  EXPECT_EQ(static_cast<int>(input.length()),
            BIO_write(bio_, input.data(), input.length()));
  EXPECT_EQ(static_cast<int>(input.length()),
            BIO_write(bio_, input.data(), input.length()));
  EXPECT_EQ(1, BIO_flush(bio_));
  ASSERT_EQ(2u, visitor_.received_messages().size());
  EXPECT_EQ("abcdeabcde", visitor_.received_messages()[1]);
}

}  // namespace
}  // namespace test
}  // namespace net
