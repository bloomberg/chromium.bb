// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_sequencer.h"

#include <utility>
#include <vector>

#include "base/rand_util.h"
#include "net/quic/reliable_quic_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::min;
using std::pair;
using std::vector;
using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Return;
using testing::StrEq;

namespace net {
namespace test {

class QuicStreamSequencerPeer : public QuicStreamSequencer {
 public:
  explicit QuicStreamSequencerPeer(ReliableQuicStream* stream)
      : QuicStreamSequencer(stream) {
  }

  QuicStreamSequencerPeer(int32 max_mem, ReliableQuicStream* stream)
      : QuicStreamSequencer(max_mem, stream) {
  }

  virtual bool OnFinFrame(QuicStreamOffset byte_offset,
                          const char* data) {
    QuicStreamFrame frame;
    frame.stream_id = 1;
    frame.offset = byte_offset;
    frame.data = StringPiece(data);
    frame.fin = true;
    return OnStreamFrame(frame);
  }

  virtual bool OnFrame(QuicStreamOffset byte_offset,
                       const char* data) {
    QuicStreamFrame frame;
    frame.stream_id = 1;
    frame.offset = byte_offset;
    frame.data = StringPiece(data);
    frame.fin = false;
    return OnStreamFrame(frame);
  }

  void SetMemoryLimit(size_t limit) {
    max_frame_memory_ = limit;
  }

  const ReliableQuicStream* stream() const { return stream_; }
  uint64 num_bytes_consumed() const { return num_bytes_consumed_; }
  const FrameMap* frames() const { return &frames_; }
  int32 max_frame_memory() const { return max_frame_memory_; }
  QuicStreamOffset close_offset() const { return close_offset_; }
};

class MockStream : public ReliableQuicStream {
 public:
  MockStream(QuicSession* session, QuicStreamId id)
      : ReliableQuicStream(id, session) {
  }

  MOCK_METHOD1(TerminateFromPeer, void(bool half_close));
  MOCK_METHOD2(ProcessData, uint32(const char* data, uint32 data_len));
  MOCK_METHOD1(Close, void(QuicRstStreamErrorCode error));
  MOCK_METHOD0(OnCanWrite, void());
};

namespace {

static const char kPayload[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

class QuicStreamSequencerTest : public ::testing::Test {
 protected:
  QuicStreamSequencerTest()
      : session_(NULL),
        stream_(session_,  1),
        sequencer_(new QuicStreamSequencerPeer(&stream_)) {
  }

  bool VerifyReadableRegions(const char** expected, size_t num_expected) {
    iovec iovecs[5];
    size_t num_iovecs = sequencer_->GetReadableRegions(iovecs,
                                                       arraysize(iovecs));
    return VerifyIovecs(iovecs, num_iovecs, expected, num_expected);
  }

  bool VerifyIovecs(iovec* iovecs,
                    size_t num_iovecs,
                    const char** expected,
                    size_t num_expected) {
    if (num_expected != num_iovecs) {
      LOG(ERROR) << "Incorrect number of iovecs.  Expected: "
                 << num_expected << " Actual: " << num_iovecs;
      return false;
    }
    for (size_t i = 0; i < num_expected; ++i) {
      if (!VerifyIovec(iovecs[i], expected[i])) {
        return false;
      }
    }
    return true;
  }

  bool VerifyIovec(const iovec& iovec, StringPiece expected) {
    if (iovec.iov_len != expected.length()) {
      LOG(ERROR) << "Invalid length: " << iovec.iov_len
                 << " vs " << expected.length();
      return false;
    }
    if (memcmp(iovec.iov_base, expected.data(), expected.length()) != 0) {
      LOG(ERROR) << "Invalid data: " << static_cast<char*>(iovec.iov_base)
                 << " vs " << expected.data();
      return false;
    }
    return true;
  }

  QuicSession* session_;
  testing::StrictMock<MockStream> stream_;
  scoped_ptr<QuicStreamSequencerPeer> sequencer_;
};

TEST_F(QuicStreamSequencerTest, RejectOldFrame) {
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3))
      .WillOnce(Return(3));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_EQ(0u, sequencer_->frames()->size());
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());
  // Ignore this - it matches a past sequence number and we should not see it
  // again.
  EXPECT_TRUE(sequencer_->OnFrame(0, "def"));
  EXPECT_EQ(0u, sequencer_->frames()->size());
}

TEST_F(QuicStreamSequencerTest, RejectOverlyLargeFrame) {
  // TODO(rch): enable when chromium supports EXPECT_DFATAL.
  /*
  EXPECT_DFATAL(sequencer_.reset(new QuicStreamSequencerPeer(2, &stream_)),
                "Setting max frame memory to 2.  "
                "Some frames will be impossible to handle.");

  EXPECT_DEBUG_DEATH(sequencer_->OnFrame(0, "abc"), "");
  */
}

TEST_F(QuicStreamSequencerTest, DropFramePastBuffering) {
  sequencer_->SetMemoryLimit(3);

  EXPECT_FALSE(sequencer_->OnFrame(3, "abc"));
}

TEST_F(QuicStreamSequencerTest, RejectBufferedFrame) {
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_EQ(1u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  // Ignore this - it matches a buffered frame.
  // Right now there's no checking that the payload is consistent.
  EXPECT_TRUE(sequencer_->OnFrame(0, "def"));
  EXPECT_EQ(1u, sequencer_->frames()->size());
}

TEST_F(QuicStreamSequencerTest, FullFrameConsumed) {
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_EQ(0u, sequencer_->frames()->size());
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, EmptyFrame) {
  EXPECT_TRUE(sequencer_->OnFrame(0, ""));
  EXPECT_EQ(0u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, EmptyFinFrame) {
  EXPECT_CALL(stream_, TerminateFromPeer(true));
  EXPECT_TRUE(sequencer_->OnFinFrame(0, ""));
  EXPECT_EQ(0u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, PartialFrameConsumed) {
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(2));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_EQ(1u, sequencer_->frames()->size());
  EXPECT_EQ(2u, sequencer_->num_bytes_consumed());
  EXPECT_EQ("c", sequencer_->frames()->find(2)->second);
}

TEST_F(QuicStreamSequencerTest, NextxFrameNotConsumed) {
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(0));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_EQ(1u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  EXPECT_EQ("abc", sequencer_->frames()->find(0)->second);
}

TEST_F(QuicStreamSequencerTest, FutureFrameNotProcessed) {
  EXPECT_TRUE(sequencer_->OnFrame(3, "abc"));
  EXPECT_EQ(1u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  EXPECT_EQ("abc", sequencer_->frames()->find(3)->second);
}

TEST_F(QuicStreamSequencerTest, OutOfOrderFrameProcessed) {
  // Buffer the first
  EXPECT_TRUE(sequencer_->OnFrame(6, "ghi"));
  EXPECT_EQ(1u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  // Buffer the second
  EXPECT_TRUE(sequencer_->OnFrame(3, "def"));
  EXPECT_EQ(2u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());

  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("def"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("ghi"), 3)).WillOnce(Return(3));

  // Ack right away
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_EQ(9u, sequencer_->num_bytes_consumed());

  EXPECT_EQ(0u, sequencer_->frames()->size());
}

TEST_F(QuicStreamSequencerTest, OutOfOrderFramesProcessedWithBuffering) {
  sequencer_->SetMemoryLimit(9);

  // Too far to buffer.
  EXPECT_FALSE(sequencer_->OnFrame(9, "jkl"));

  // We can afford to buffer this.
  EXPECT_TRUE(sequencer_->OnFrame(6, "ghi"));
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());

  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));

  // Ack right away
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());

  // We should be willing to buffer this now.
  EXPECT_TRUE(sequencer_->OnFrame(9, "jkl"));
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());

  EXPECT_CALL(stream_, ProcessData(StrEq("def"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("ghi"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("jkl"), 3)).WillOnce(Return(3));

  EXPECT_TRUE(sequencer_->OnFrame(3, "def"));
  EXPECT_EQ(12u, sequencer_->num_bytes_consumed());
  EXPECT_EQ(0u, sequencer_->frames()->size());
}

TEST_F(QuicStreamSequencerTest, OutOfOrderFramesBlockignWithReadv) {
  sequencer_->SetMemoryLimit(9);
  char buffer[20];
  iovec iov[2];
  iov[0].iov_base = &buffer[0];
  iov[0].iov_len = 1;
  iov[1].iov_base = &buffer[1];
  iov[1].iov_len = 2;

  // Push abc - process.
  // Push jkl - buffer (not next data)
  // Push def - don't process.
  // Push mno - drop (too far out)
  // Push ghi - buffer (def not processed)
  // Read 2.
  // Push mno - buffer (not all read)
  // Read all
  // Push pqr - process

  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("def"), 3)).WillOnce(Return(0));
  EXPECT_CALL(stream_, ProcessData(StrEq("pqr"), 3)).WillOnce(Return(3));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_TRUE(sequencer_->OnFrame(3, "def"));
  EXPECT_TRUE(sequencer_->OnFrame(9, "jkl"));
  EXPECT_FALSE(sequencer_->OnFrame(12, "mno"));
  EXPECT_TRUE(sequencer_->OnFrame(6, "ghi"));

  // Read 3 bytes.
  EXPECT_EQ(3, sequencer_->Readv(iov, 2));
  EXPECT_EQ(0, strncmp(buffer, "def", 3));

  // Now we have space to bufer this.
  EXPECT_TRUE(sequencer_->OnFrame(12, "mno"));

  // Read the remaining 9 bytes.
  iov[1].iov_len = 19;
  EXPECT_EQ(9, sequencer_->Readv(iov, 2));
  EXPECT_EQ(0, strncmp(buffer, "ghijklmno", 9));

  EXPECT_TRUE(sequencer_->OnFrame(15, "pqr"));
}

// Same as above, just using a different method for reading.
TEST_F(QuicStreamSequencerTest, OutOfOrderFramesBlockignWithGetReadableRegion) {
  sequencer_->SetMemoryLimit(9);

  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("def"), 3)).WillOnce(Return(0));
  EXPECT_CALL(stream_, ProcessData(StrEq("pqr"), 3)).WillOnce(Return(3));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_TRUE(sequencer_->OnFrame(3, "def"));
  EXPECT_TRUE(sequencer_->OnFrame(9, "jkl"));
  EXPECT_FALSE(sequencer_->OnFrame(12, "mno"));
  EXPECT_TRUE(sequencer_->OnFrame(6, "ghi"));

  // Read 3 bytes.
  const char* expected[] = {"def", "ghi", "jkl"};
  ASSERT_TRUE(VerifyReadableRegions(expected, arraysize(expected)));
  char buffer[9];
  iovec read_iov = { &buffer[0], 3 };
  ASSERT_EQ(3, sequencer_->Readv(&read_iov, 1));

  // Now we have space to bufer this.
  EXPECT_TRUE(sequencer_->OnFrame(12, "mno"));

  // Read the remaining 9 bytes.
  const char* expected2[] = {"ghi", "jkl", "mno"};
  ASSERT_TRUE(VerifyReadableRegions(expected2, arraysize(expected2)));
  read_iov.iov_len = 9;
  ASSERT_EQ(9, sequencer_->Readv(&read_iov, 1));

  EXPECT_TRUE(sequencer_->OnFrame(15, "pqr"));
}

// Same as above, just using a different method for reading.
TEST_F(QuicStreamSequencerTest, MarkConsumed) {
  sequencer_->SetMemoryLimit(9);

  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(0));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_TRUE(sequencer_->OnFrame(3, "def"));
  EXPECT_TRUE(sequencer_->OnFrame(6, "ghi"));

  // Peek into the data.
  const char* expected[] = {"abc", "def", "ghi"};
  ASSERT_TRUE(VerifyReadableRegions(expected, arraysize(expected)));

  // Consume 1 byte.
  sequencer_->MarkConsumed(1);
  // Verify data.
  const char* expected2[] = {"bc", "def", "ghi"};
  ASSERT_TRUE(VerifyReadableRegions(expected2, arraysize(expected2)));

  // Consume 2 bytes.
  sequencer_->MarkConsumed(2);
  // Verify data.
  const char* expected3[] = {"def", "ghi"};
  ASSERT_TRUE(VerifyReadableRegions(expected3, arraysize(expected3)));

  // Consume 5 bytes.
  sequencer_->MarkConsumed(5);
  // Verify data.
  const char* expected4[] = {"i"};
  ASSERT_TRUE(VerifyReadableRegions(expected4, arraysize(expected4)));
}

TEST_F(QuicStreamSequencerTest, MarkConsumedError) {
  // TODO(rch): enable when chromium supports EXPECT_DFATAL.
  /*
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(0));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_TRUE(sequencer_->OnFrame(9, "jklmnopqrstuvwxyz"));

  // Peek into the data.  Only the first chunk should be readable
  // because of the missing data.
  const char* expected[] = {"abc"};
  ASSERT_TRUE(VerifyReadableRegions(expected, arraysize(expected)));

  // Now, attempt to mark consumed more data than was readable
  // and expect the stream to be closed.
  EXPECT_CALL(stream_, Close(QUIC_SERVER_ERROR_PROCESSING_STREAM));
  EXPECT_DFATAL(sequencer_->MarkConsumed(4),
                "Invalid argument to MarkConsumed.  num_bytes_consumed_: 3 "
                "end_offset: 4 offset: 9 length: 17");
  */
}

TEST_F(QuicStreamSequencerTest, MarkConsumedWithMissingPacket) {
  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(0));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
  EXPECT_TRUE(sequencer_->OnFrame(3, "def"));
  // Missing packet: 6, ghi
  EXPECT_TRUE(sequencer_->OnFrame(9, "jkl"));

  const char* expected[] = {"abc", "def"};
  ASSERT_TRUE(VerifyReadableRegions(expected, arraysize(expected)));

  sequencer_->MarkConsumed(6);
}

TEST_F(QuicStreamSequencerTest, BasicHalfCloseOrdered) {
  InSequence s;

  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, TerminateFromPeer(true));
  EXPECT_TRUE(sequencer_->OnFinFrame(0, "abc"));

  EXPECT_EQ(3u, sequencer_->close_offset());
}

TEST_F(QuicStreamSequencerTest, BasicHalfCloseUnorderedWithFlush) {
  sequencer_->OnFinFrame(6, "");
  EXPECT_EQ(6u, sequencer_->close_offset());
  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("def"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, TerminateFromPeer(true));

  EXPECT_TRUE(sequencer_->OnFrame(3, "def"));
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
}

TEST_F(QuicStreamSequencerTest, BasicHalfUnordered) {
  sequencer_->OnFinFrame(3, "");
  EXPECT_EQ(3u, sequencer_->close_offset());
  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, TerminateFromPeer(true));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));
}

TEST_F(QuicStreamSequencerTest, TerminateWithReadv) {
  char buffer[3];

  sequencer_->OnFinFrame(3, "");
  EXPECT_EQ(3u, sequencer_->close_offset());

  EXPECT_FALSE(sequencer_->IsHalfClosed());

  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(0));
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc"));

  iovec iov = { &buffer[0], 3 };
  int bytes_read = sequencer_->Readv(&iov, 1);
  EXPECT_EQ(3, bytes_read);
  EXPECT_TRUE(sequencer_->IsHalfClosed());
}

TEST_F(QuicStreamSequencerTest, MutipleOffsets) {
  sequencer_->OnFinFrame(3, "");
  EXPECT_EQ(3u, sequencer_->close_offset());

  EXPECT_CALL(stream_, Close(QUIC_MULTIPLE_TERMINATION_OFFSETS));
  sequencer_->OnFinFrame(5, "");
  EXPECT_EQ(3u, sequencer_->close_offset());

  EXPECT_CALL(stream_, Close(QUIC_MULTIPLE_TERMINATION_OFFSETS));
  sequencer_->OnFinFrame(1, "");
  EXPECT_EQ(3u, sequencer_->close_offset());

  sequencer_->OnFinFrame(3, "");
  EXPECT_EQ(3u, sequencer_->close_offset());
}

class QuicSequencerRandomTest : public QuicStreamSequencerTest {
 public:
  typedef pair<int, string> Frame;
  typedef vector<Frame> FrameList;

  void CreateFrames() {
    int payload_size = arraysize(kPayload) - 1;
    int remaining_payload = payload_size;
    while (remaining_payload != 0) {
      int size = min(OneToN(6), remaining_payload);
      int index = payload_size - remaining_payload;
      list_.push_back(make_pair(index, string(kPayload + index, size)));
      remaining_payload -= size;
    }
  }

  QuicSequencerRandomTest() {
    CreateFrames();
  }

  int OneToN(int n) {
    return base::RandInt(1, n);
  }

  int MaybeProcessMaybeBuffer(const char* data, uint32 len) {
    int to_process = len;
    if (base::RandUint64() % 2 != 0) {
      to_process = base::RandInt(0, len);
    }
    output_.append(data, to_process);
    return to_process;
  }

  string output_;
  FrameList list_;
};

// All frames are processed as soon as we have sequential data.
// Infinite buffering, so all frames are acked right away.
TEST_F(QuicSequencerRandomTest, RandomFramesNoDroppingNoBackup) {
  InSequence s;
  for (size_t i = 0; i < list_.size(); ++i) {
    string* data = &list_[i].second;
    EXPECT_CALL(stream_, ProcessData(StrEq(*data), data->size()))
        .WillOnce(Return(data->size()));
  }

  while (list_.size() != 0) {
    int index = OneToN(list_.size()) - 1;
    LOG(ERROR) << "Sending index " << index << " "
               << list_[index].second.data();
    EXPECT_TRUE(sequencer_->OnFrame(list_[index].first,
                                    list_[index].second.data()));

    list_.erase(list_.begin() + index);
  }
}

// All frames are processed as soon as we have sequential data.
// Buffering, so some frames are rejected.
TEST_F(QuicSequencerRandomTest, RandomFramesDroppingNoBackup) {
  sequencer_->SetMemoryLimit(26);

  InSequence s;
  for (size_t i = 0; i < list_.size(); ++i) {
    string* data = &list_[i].second;
    EXPECT_CALL(stream_, ProcessData(StrEq(*data), data->size()))
        .WillOnce(Return(data->size()));
  }

  while (list_.size() != 0) {
    int index = OneToN(list_.size()) - 1;
    LOG(ERROR) << "Sending index " << index << " "
               << list_[index].second.data();
    bool acked = sequencer_->OnFrame(list_[index].first,
                                     list_[index].second.data());

    if (acked) {
      list_.erase(list_.begin() + index);
    }
  }
}

}  // namespace
}  // namespace test
}  // namespace net
