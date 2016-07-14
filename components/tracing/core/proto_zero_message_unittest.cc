// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/proto_zero_message.h"

#include <limits>
#include <memory>
#include <vector>

#include "base/hash.h"
#include "components/tracing/core/proto_utils.h"
#include "components/tracing/test/fake_scattered_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace v2 {

const size_t kChunkSize = 16;
const uint8_t kTestBytes[] = {0, 0, 0, 0, 0x42, 1, 0x42, 0xff, 0x42, 0};
const char kStartWatermark[] = {'a', 'b', 'c', 'd', '1', '2', '3', '\0'};
const char kEndWatermark[] = {'9', '8', '7', '6', 'z', 'w', 'y', '\0'};

class FakeMessage : public ProtoZeroMessage {
 public:
};

class ProtoZeroMessageTest : public ::testing::Test {
 public:
  void SetUp() override {
    buffer_.reset(new FakeScatteredBuffer(kChunkSize));
    stream_writer_.reset(new ScatteredStreamWriter(buffer_.get()));
    readback_pos_ = 0;
  }

  void TearDown() override {
    // Check that none of the messages created by the text fixtures below did
    // under/overflow their heap boundaries.
    for (std::unique_ptr<uint8_t[]>& mem : messages_) {
      EXPECT_STREQ(kStartWatermark, reinterpret_cast<char*>(mem.get()));
      EXPECT_STREQ(kEndWatermark,
                   reinterpret_cast<char*>(mem.get() + sizeof(kStartWatermark) +
                                           sizeof(ProtoZeroMessage)));
      mem.reset();
    }
    messages_.clear();
    stream_writer_.reset();
    buffer_.reset();
  }

  ProtoZeroMessage* NewMessage() {
    std::unique_ptr<uint8_t[]> mem(
        new uint8_t[sizeof(kStartWatermark) + sizeof(ProtoZeroMessage) +
                    sizeof(kEndWatermark)]);
    uint8_t* msg_start = mem.get() + sizeof(kStartWatermark);
    memcpy(mem.get(), kStartWatermark, sizeof(kStartWatermark));
    memset(msg_start, 0, sizeof(ProtoZeroMessage));
    memcpy(msg_start + sizeof(ProtoZeroMessage), kEndWatermark,
           sizeof(kEndWatermark));
    messages_.push_back(std::move(mem));
    ProtoZeroMessage* msg = reinterpret_cast<ProtoZeroMessage*>(msg_start);
    msg->Reset(stream_writer_.get());
    return msg;
  }

  size_t GetNumSerializedBytes() {
    if (buffer_->chunks().empty())
      return 0;
    return buffer_->chunks().size() * kChunkSize -
           stream_writer_->bytes_available();
  }

  std::string GetNextSerializedBytes(size_t num_bytes) {
    size_t old_readback_pos = readback_pos_;
    readback_pos_ += num_bytes;
    return buffer_->GetBytesAsString(old_readback_pos, num_bytes);
  }

  static void BuildNestedMessages(uint32_t depth, ProtoZeroMessage* msg) {
    for (uint32_t i = 1; i <= 128; ++i)
      msg->AppendBytes(i, kTestBytes, sizeof(kTestBytes));

    if (depth < ProtoZeroMessage::kMaxNestingDepth) {
      auto* child_msg = msg->BeginNestedMessage<FakeMessage>(1 + depth * 10);
      BuildNestedMessages(depth + 1, child_msg);
    }

    for (uint32_t i = 129; i <= 256; ++i)
      msg->AppendVarIntU32(i, 42);

    if ((depth & 2) == 0)
      msg->Finalize();
  }

 private:
  std::unique_ptr<FakeScatteredBuffer> buffer_;
  std::unique_ptr<ScatteredStreamWriter> stream_writer_;
  std::vector<std::unique_ptr<uint8_t[]>> messages_;
  size_t readback_pos_;
};

TEST_F(ProtoZeroMessageTest, BasicTypesNoNesting) {
  ProtoZeroMessage* msg = NewMessage();
  msg->AppendVarIntU32(1 /* field_id */, 0);
  msg->AppendVarIntU32(2 /* field_id */, std::numeric_limits<uint32_t>::max());
  msg->AppendVarIntU64(3 /* field_id */, 42);
  msg->AppendVarIntU64(4 /* field_id */, std::numeric_limits<uint64_t>::max());
  msg->AppendFloat(5 /* field_id */, 3.1415f);
  msg->AppendDouble(6 /* field_id */, 3.14159265358979323846);
  msg->AppendBytes(7 /* field_id */, kTestBytes, sizeof(kTestBytes));

  // Field ids > 16 are expected to be varint encoded (preamble > 1 byte)
  msg->AppendString(257 /* field_id */, "0123456789abcdefABCDEF");

  EXPECT_EQ(72u, msg->Finalize());
  EXPECT_EQ(72u, GetNumSerializedBytes());

  // These lines match the serialization of the Append* calls above.
  ASSERT_EQ("0800", GetNextSerializedBytes(2));
  ASSERT_EQ("10FFFFFFFF0F", GetNextSerializedBytes(6));
  ASSERT_EQ("182A", GetNextSerializedBytes(2));
  ASSERT_EQ("20FFFFFFFFFFFFFFFFFF01", GetNextSerializedBytes(11));
  ASSERT_EQ("2D560E4940", GetNextSerializedBytes(5));
  ASSERT_EQ("31182D4454FB210940", GetNextSerializedBytes(9));
  ASSERT_EQ("3A0A00000000420142FF4200", GetNextSerializedBytes(12));
  ASSERT_EQ("8A101630313233343536373839616263646566414243444546",
            GetNextSerializedBytes(25));
}

TEST_F(ProtoZeroMessageTest, NestedMessagesSimple) {
  ProtoZeroMessage* root_msg = NewMessage();
  root_msg->AppendVarIntU32(1 /* field_id */, 1);

  FakeMessage* nested_msg =
      root_msg->BeginNestedMessage<FakeMessage>(128 /* field_id */);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(nested_msg) % sizeof(void*));
  nested_msg->AppendVarIntU32(2 /* field_id */, 2);

  nested_msg = root_msg->BeginNestedMessage<FakeMessage>(129 /* field_id */);
  nested_msg->AppendVarIntU32(4 /* field_id */, 2);

  root_msg->AppendVarIntU32(5 /* field_id */, 3);

  // The expected size of the root message is supposed to be 20 bytes:
  //   2 bytes for the varint field (id: 1) (1 for preamble and one for payload)
  //   6 bytes for the preamble of the 1st nested message (2 for id, 4 for size)
  //   2 bytes for the varint field (id: 2) of the 1st nested message
  //   6 bytes for the premable of the 2nd nested message
  //   2 bytes for the varint field (id: 4) of the 2nd nested message.
  //   2 bytes for the last varint (id : 5) field of the root message.
  // Test also that finalization is idempontent and Finalize() can be safely
  // called more than once without side effects.
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(20u, root_msg->Finalize());
    EXPECT_EQ(20u, GetNumSerializedBytes());
  }

  ASSERT_EQ("0801", GetNextSerializedBytes(2));

  ASSERT_EQ("820882808000", GetNextSerializedBytes(6));
  ASSERT_EQ("1002", GetNextSerializedBytes(2));

  ASSERT_EQ("8A0882808000", GetNextSerializedBytes(6));
  ASSERT_EQ("2002", GetNextSerializedBytes(2));

  ASSERT_EQ("2803", GetNextSerializedBytes(2));
}

// Checks that the size field of root and nested messages is properly written
// on finalization.
TEST_F(ProtoZeroMessageTest, BackfillSizeOnFinalization) {
  ProtoZeroMessage* root_msg = NewMessage();
  uint8_t root_msg_size[proto::kMessageLengthFieldSize];
  root_msg->set_size_field(
      {&root_msg_size[0], &root_msg_size[proto::kMessageLengthFieldSize]});
  root_msg->AppendVarIntU32(1, 0x42);

  FakeMessage* nested_msg_1 = root_msg->BeginNestedMessage<FakeMessage>(2);
  nested_msg_1->AppendVarIntU32(3, 0x43);

  FakeMessage* nested_msg_2 = nested_msg_1->BeginNestedMessage<FakeMessage>(4);
  uint8_t buf200[200];
  memset(buf200, 0x42, sizeof(buf200));
  nested_msg_2->AppendBytes(5, buf200, sizeof(buf200));

  root_msg->inc_size_already_written(6);

  // The value returned by Finalize() should be == the full size of |root_msg|.
  EXPECT_EQ(217u, root_msg->Finalize());
  EXPECT_EQ(217u, GetNumSerializedBytes());

  // However the size written in the size field should take into account the
  // inc_size_already_written() call and be equal to 118 - 6 = 112, encoded
  // in a rendundant varint encoding of kMessageLengthFieldSize bytes.
  EXPECT_STREQ("\xD3\x81\x80\x00", reinterpret_cast<char*>(root_msg_size));

  // Skip 2 bytes for the 0x42 varint + 1 byte for the |nested_msg_1| preamble.
  GetNextSerializedBytes(3);

  // Check that the size of |nested_msg_1| was backfilled. Its size is:
  // 203 bytes for |nest_mesg_2| (see below) + 5 bytes for its preamble +
  // 2 bytes for the 0x43 varint = 210 bytes.
  EXPECT_EQ("D2818000", GetNextSerializedBytes(4));

  // Skip 2 bytes for the 0x43 varint + 1 byte for the |nested_msg_2| preamble.
  GetNextSerializedBytes(3);

  // Check that the size of |nested_msg_2| was backfilled. Its size is:
  // 200 bytes (for |buf200|) + 3 bytes for its preamble = 203 bytes.
  EXPECT_EQ("CB818000", GetNextSerializedBytes(4));
}

TEST_F(ProtoZeroMessageTest, StressTest) {
  std::vector<ProtoZeroMessage*> nested_msgs;

  ProtoZeroMessage* root_msg = NewMessage();
  BuildNestedMessages(0, root_msg);
  root_msg->Finalize();

  // The main point of this test is to stress the code paths and test for
  // unexpected crashes of the production code. The actual serialization is
  // already covered in the other text fixtures. Keeping just a final smoke test
  // here on the full buffer hash.
  std::string full_buf = GetNextSerializedBytes(GetNumSerializedBytes());
  uint32_t buf_hash = base::SuperFastHash(full_buf.data(), full_buf.size());
  EXPECT_EQ(0x14BC1BA3u, buf_hash);
}

}  // namespace v2
}  // namespace tracing
