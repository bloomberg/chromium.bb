// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iostream>
#include <limits>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

using std::string;
using std::max;
using std::min;
using std::numeric_limits;
using testing::_;

namespace net {

namespace test {

static const size_t kMaxDecompressedSize = 1024;

// TODO(akalin): Make sure expectations on mocks are set before mock
// functions are called, as interleaving expectations and calls is
// undefined.
class MockVisitor : public SpdyFramerVisitorInterface {
 public:
  MOCK_METHOD1(OnError, void(SpdyFramer* framer));
  MOCK_METHOD6(OnSynStream, void(SpdyStreamId stream_id,
                                 SpdyStreamId associated_stream_id,
                                 SpdyPriority priority,
                                 uint8 slot,
                                 bool fin,
                                 bool unidirectional));
  MOCK_METHOD2(OnSynReply, void(SpdyStreamId stream_id, bool fin));
  MOCK_METHOD2(OnHeaders, void(SpdyStreamId stream_id, bool fin));
  MOCK_METHOD3(OnControlFrameHeaderData, bool(SpdyStreamId stream_id,
                                              const char* header_data,
                                              size_t len));
  MOCK_METHOD2(OnCredentialFrameData, bool(const char* credential_data,
                                           size_t len));
  MOCK_METHOD3(OnDataFrameHeader, void(SpdyStreamId stream_id,
                                       size_t length,
                                       bool fin));
  MOCK_METHOD4(OnStreamFrameData, void(SpdyStreamId stream_id,
                                       const char* data,
                                       size_t len,
                                       SpdyDataFlags flags));
  MOCK_METHOD3(OnSetting, void(SpdySettingsIds id, uint8 flags, uint32 value));
  MOCK_METHOD1(OnPing, void(uint32 unique_id));
  MOCK_METHOD2(OnRstStream, void(SpdyStreamId stream_id,
                                 SpdyRstStreamStatus status));
  MOCK_METHOD2(OnGoAway, void(SpdyStreamId last_accepted_stream_id,
                              SpdyGoAwayStatus status));
  MOCK_METHOD2(OnWindowUpdate, void(SpdyStreamId stream_id,
                                    int delta_window_size));
  MOCK_METHOD2(OnSynStreamCompressed,
               void(size_t uncompressed_length,
                    size_t compressed_length));
};

class SpdyFramerTestUtil {
 public:
  // Decompress a single frame using the decompression context held by
  // the SpdyFramer.  The implemention is meant for use only in tests
  // and will CHECK fail if the input is anything other than a single,
  // well-formed compressed frame.
  //
  // Returns a new decompressed SpdyFrame.
  template<class SpdyFrameType> static SpdyFrame* DecompressFrame(
      SpdyFramer* framer, const SpdyFrameType& frame) {
    DecompressionVisitor visitor(framer->protocol_version());
    framer->set_visitor(&visitor);
    size_t input_size = frame.size();
    CHECK_EQ(input_size, framer->ProcessInput(frame.data(), input_size));
    CHECK_EQ(SpdyFramer::SPDY_RESET, framer->state());
    framer->set_visitor(NULL);

    char* buffer = visitor.ReleaseBuffer();
    CHECK(buffer != NULL);
    SpdyFrame* decompressed_frame = new SpdyFrame(buffer, visitor.size(), true);
    SetFrameLength(decompressed_frame,
                   visitor.size() - framer->GetControlFrameHeaderSize(),
                   framer->protocol_version());
    return decompressed_frame;
  }

  class DecompressionVisitor : public SpdyFramerVisitorInterface {
   public:
    explicit DecompressionVisitor(int version)
        : version_(version), buffer_(NULL), size_(0), finished_(false) {
    }

    void ResetBuffer() {
      CHECK(buffer_.get() == NULL);
      CHECK_EQ(0u, size_);
      CHECK(!finished_);
      buffer_.reset(new char[kMaxDecompressedSize]);
    }

    virtual void OnSynStream(SpdyStreamId stream_id,
                             SpdyStreamId associated_stream_id,
                             SpdyPriority priority,
                             uint8 slot,
                             bool fin,
                             bool unidirectional) OVERRIDE {
      SpdyFramer framer(version_);
      framer.set_enable_compression(false);
      const SpdyHeaderBlock null_headers;
      int flags = CONTROL_FLAG_NONE;
      if (fin) {
        flags &= CONTROL_FLAG_FIN;
      }
      if (unidirectional) {
        flags &= CONTROL_FLAG_UNIDIRECTIONAL;
      }
      scoped_ptr<SpdyFrame> frame(
          framer.CreateSynStream(stream_id,
                                 associated_stream_id,
                                 priority,
                                 slot,
                                 static_cast<SpdyControlFlags>(flags),
                                 false,
                                 &null_headers));
      ResetBuffer();
      memcpy(buffer_.get(), frame->data(), framer.GetSynStreamMinimumSize());
      size_ += framer.GetSynStreamMinimumSize();
    }

    virtual void OnSynReply(SpdyStreamId stream_id, bool fin) OVERRIDE {
      SpdyFramer framer(version_);
      framer.set_enable_compression(false);
      const SpdyHeaderBlock null_headers;
      int flags = CONTROL_FLAG_NONE;
      if (fin) {
        flags &= CONTROL_FLAG_FIN;
      }
      scoped_ptr<SpdyFrame> frame(
          framer.CreateHeaders(stream_id,
                               static_cast<SpdyControlFlags>(flags),
                               false,
                               &null_headers));
      ResetBuffer();
      memcpy(buffer_.get(), frame->data(), framer.GetHeadersMinimumSize());
      size_ += framer.GetSynStreamMinimumSize();
    }

    virtual void OnHeaders(SpdyStreamId stream_id, bool fin) OVERRIDE {
      SpdyFramer framer(version_);
      framer.set_enable_compression(false);
      const SpdyHeaderBlock null_headers;
      int flags = CONTROL_FLAG_NONE;
      if (fin) {
        flags &= CONTROL_FLAG_FIN;
      }
      scoped_ptr<SpdyFrame> frame(
          framer.CreateHeaders(stream_id,
                               static_cast<SpdyControlFlags>(flags),
                               false,
                               &null_headers));
      ResetBuffer();
      memcpy(buffer_.get(), frame->data(), framer.GetHeadersMinimumSize());
      size_ += framer.GetSynStreamMinimumSize();
    }

    virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                          const char* header_data,
                                          size_t len) OVERRIDE {
      CHECK(buffer_.get() != NULL);
      CHECK_GE(kMaxDecompressedSize, size_ + len);
      CHECK(!finished_);
      if (len != 0) {
        memcpy(buffer_.get() + size_, header_data, len);
        size_ += len;
      } else {
        // Done.
        finished_ = true;
      }
      return true;
    }

    virtual bool OnCredentialFrameData(const char* /*credential_data*/,
                                       size_t /*len*/) OVERRIDE {
      LOG(FATAL) << "Unexpected CREDENTIAL Frame";
      return false;
    }

    virtual void OnError(SpdyFramer* framer) OVERRIDE { LOG(FATAL); }
    virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                   size_t length,
                                   bool fin) OVERRIDE {
      LOG(FATAL) << "Unexpected data frame header";
    }
    virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                   const char* data,
                                   size_t len,
                                   SpdyDataFlags flags) OVERRIDE {
      LOG(FATAL);
    }
    virtual void OnSetting(SpdySettingsIds id,
                           uint8 flags,
                           uint32 value) OVERRIDE {
      LOG(FATAL);
    }
    virtual void OnSynStreamCompressed(
        size_t uncompressed_size,
        size_t compressed_size) OVERRIDE {
    }
    virtual void OnPing(uint32 unique_id) OVERRIDE {
      LOG(FATAL);
    }
    virtual void OnRstStream(SpdyStreamId stream_id,
                             SpdyRstStreamStatus status) OVERRIDE {
      LOG(FATAL);
    }
    virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                          SpdyGoAwayStatus status) OVERRIDE {
      LOG(FATAL);
    }
    virtual void OnWindowUpdate(SpdyStreamId stream_id,
                                int delta_window_size) OVERRIDE {
      LOG(FATAL);
    }

    char* ReleaseBuffer() {
      CHECK(finished_);
      return buffer_.release();
    }

    size_t size() const {
      CHECK(finished_);
      return size_;
    }

   private:
    int version_;
    scoped_array<char> buffer_;
    size_t size_;
    bool finished_;

    DISALLOW_COPY_AND_ASSIGN(DecompressionVisitor);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyFramerTestUtil);
};

class TestSpdyVisitor : public SpdyFramerVisitorInterface,
                        public SpdyFramerDebugVisitorInterface {
 public:
  static const size_t kDefaultHeaderBufferSize = 16 * 1024 * 1024;
  static const size_t kDefaultCredentialBufferSize = 16 * 1024;

  explicit TestSpdyVisitor(int version)
    : framer_(version),
      use_compression_(false),
      error_count_(0),
      syn_frame_count_(0),
      syn_reply_frame_count_(0),
      headers_frame_count_(0),
      goaway_count_(0),
      setting_count_(0),
      data_bytes_(0),
      fin_frame_count_(0),
      fin_flag_count_(0),
      zero_length_data_frame_count_(0),
      control_frame_header_data_count_(0),
      zero_length_control_frame_header_data_count_(0),
      data_frame_count_(0),
      last_uncompressed_size_(0),
      last_compressed_size_(0),
      header_buffer_(new char[kDefaultHeaderBufferSize]),
      header_buffer_length_(0),
      header_buffer_size_(kDefaultHeaderBufferSize),
      header_stream_id_(-1),
      header_control_type_(NUM_CONTROL_FRAME_TYPES),
      header_buffer_valid_(false),
      credential_buffer_(new char[kDefaultCredentialBufferSize]),
      credential_buffer_length_(0),
      credential_buffer_size_(kDefaultCredentialBufferSize) {
  }

  virtual void OnError(SpdyFramer* f) OVERRIDE {
    LOG(INFO) << "SpdyFramer Error: "
              << SpdyFramer::ErrorCodeToString(f->error_code());
    error_count_++;
  }

  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) OVERRIDE {
    data_frame_count_++;
    header_stream_id_ = stream_id;
  }

  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 SpdyDataFlags flags) OVERRIDE {
    EXPECT_EQ(header_stream_id_, stream_id);
    if (len == 0)
      ++zero_length_data_frame_count_;

    data_bytes_ += len;
    std::cerr << "OnStreamFrameData(" << stream_id << ", \"";
    if (len > 0) {
      for (size_t i = 0 ; i < len; ++i) {
        std::cerr << std::hex << (0xFF & (unsigned int)data[i]) << std::dec;
      }
    }
    std::cerr << "\", " << len << ")\n";
  }

  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           uint8 credential_slot,
                           bool fin,
                           bool unidirectional) OVERRIDE {
    syn_frame_count_++;
    InitHeaderStreaming(SYN_STREAM, stream_id);
    if (fin) {
      fin_flag_count_++;
    }
  }

  virtual void OnSynReply(SpdyStreamId stream_id, bool fin) OVERRIDE {
    syn_reply_frame_count_++;
    InitHeaderStreaming(HEADERS, stream_id);
    if (fin) {
      fin_flag_count_++;
    }
  }

  virtual void OnHeaders(SpdyStreamId stream_id, bool fin) OVERRIDE {
    headers_frame_count_++;
    InitHeaderStreaming(SYN_REPLY, stream_id);
    if (fin) {
      fin_flag_count_++;
    }
  }

  virtual void OnSetting(SpdySettingsIds id,
                         uint8 flags,
                         uint32 value) OVERRIDE {
    setting_count_++;
  }

  virtual void OnSynStreamCompressed(
      size_t uncompressed_size,
      size_t compressed_size) OVERRIDE {
  }

  virtual void OnPing(uint32 unique_id) OVERRIDE {
    DLOG(FATAL);
  }

  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE {
    fin_frame_count_++;
  }

  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE {
    goaway_count_++;
  }

  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              int delta_window_size) OVERRIDE {
    last_window_update_stream_ = stream_id;
    last_window_update_delta_ = delta_window_size;
  }

  virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) OVERRIDE {
    ++control_frame_header_data_count_;
    CHECK_EQ(header_stream_id_, stream_id);
    if (len == 0) {
      ++zero_length_control_frame_header_data_count_;
      // Indicates end-of-header-block.
      CHECK(header_buffer_valid_);
      size_t parsed_length = framer_.ParseHeaderBlockInBuffer(
          header_buffer_.get(), header_buffer_length_, &headers_);
      DCHECK_EQ(header_buffer_length_, parsed_length);
      return true;
    }
    const size_t available = header_buffer_size_ - header_buffer_length_;
    if (len > available) {
      header_buffer_valid_ = false;
      return false;
    }
    memcpy(header_buffer_.get() + header_buffer_length_, header_data, len);
    header_buffer_length_ += len;
    return true;
  }

  virtual bool OnCredentialFrameData(const char* credential_data,
                                     size_t len) OVERRIDE {
    if (len == 0) {
      if (!framer_.ParseCredentialData(credential_buffer_.get(),
                                       credential_buffer_length_,
                                       &credential_)) {
        ++error_count_;
      }
      return true;
    }
    const size_t available =
        credential_buffer_size_ - credential_buffer_length_;
    if (len > available) {
      return false;
    }
    memcpy(credential_buffer_.get() + credential_buffer_length_,
           credential_data, len);
    credential_buffer_length_ += len;
    return true;
  }

  virtual void OnCompressedHeaderBlock(size_t uncompressed_size,
                                       size_t compressed_size) OVERRIDE {
    last_uncompressed_size_ = uncompressed_size;
    last_compressed_size_ = compressed_size;
  }

  virtual void OnDecompressedHeaderBlock(size_t decompressed_size,
                                         size_t compressed_size) OVERRIDE {
    last_uncompressed_size_ = decompressed_size;
    last_compressed_size_ = compressed_size;
  }

  // Convenience function which runs a framer simulation with particular input.
  void SimulateInFramer(const unsigned char* input, size_t size) {
    framer_.set_enable_compression(use_compression_);
    framer_.set_visitor(this);
    size_t input_remaining = size;
    const char* input_ptr = reinterpret_cast<const char*>(input);
    while (input_remaining > 0 &&
           framer_.error_code() == SpdyFramer::SPDY_NO_ERROR) {
      // To make the tests more interesting, we feed random (amd small) chunks
      // into the framer.  This simulates getting strange-sized reads from
      // the socket.
      const size_t kMaxReadSize = 32;
      size_t bytes_read =
          (rand() % min(input_remaining, kMaxReadSize)) + 1;
      size_t bytes_processed = framer_.ProcessInput(input_ptr, bytes_read);
      input_remaining -= bytes_processed;
      input_ptr += bytes_processed;
    }
  }

  void InitHeaderStreaming(SpdyControlType header_control_type,
                           SpdyStreamId stream_id) {
    memset(header_buffer_.get(), 0, header_buffer_size_);
    header_buffer_length_ = 0;
    header_stream_id_ = stream_id;
    header_control_type_ = header_control_type;
    header_buffer_valid_ = true;
    DCHECK_NE(header_stream_id_, SpdyFramer::kInvalidStream);
  }

  // Override the default buffer size (16K). Call before using the framer!
  void set_header_buffer_size(size_t header_buffer_size) {
    header_buffer_size_ = header_buffer_size;
    header_buffer_.reset(new char[header_buffer_size]);
  }

  size_t control_frame_buffer_max_size() const {
     return framer_.GetControlFrameBufferMaxSize();
  }

  static size_t header_data_chunk_max_size() {
    return SpdyFramer::kHeaderDataChunkMaxSize;
  }

  SpdyFramer framer_;
  bool use_compression_;

  // Counters from the visitor callbacks.
  int error_count_;
  int syn_frame_count_;
  int syn_reply_frame_count_;
  int headers_frame_count_;
  int goaway_count_;
  int setting_count_;
  int last_window_update_stream_;
  int last_window_update_delta_;
  int data_bytes_;
  int fin_frame_count_;  // The count of RST_STREAM type frames received.
  int fin_flag_count_;  // The count of frames with the FIN flag set.
  int zero_length_data_frame_count_;  // The count of zero-length data frames.
  int control_frame_header_data_count_;  // The count of chunks received.
  // The count of zero-length control frame header data chunks received.
  int zero_length_control_frame_header_data_count_;
  int data_frame_count_;
  size_t last_uncompressed_size_;
  size_t last_compressed_size_;

  // Header block streaming state:
  scoped_array<char> header_buffer_;
  size_t header_buffer_length_;
  size_t header_buffer_size_;
  SpdyStreamId header_stream_id_;
  SpdyControlType header_control_type_;
  bool header_buffer_valid_;
  SpdyHeaderBlock headers_;

  scoped_array<char> credential_buffer_;
  size_t credential_buffer_length_;
  size_t credential_buffer_size_;
  SpdyCredential credential_;
};

// Retrieves serialized headers from SYN_STREAM frame.
// Does not check that the given frame is a SYN_STREAM.
base::StringPiece GetSerializedHeaders(const SpdyFrame* frame,
                                       const SpdyFramer& framer) {
  return base::StringPiece(frame->data() + framer.GetSynStreamMinimumSize(),
                           frame->size() - framer.GetSynStreamMinimumSize());
}

}  // namespace test

}  // namespace net

using net::test::SetFrameLength;
using net::test::SetFrameFlags;
using net::test::CompareCharArraysWithHexError;
using net::test::SpdyFramerTestUtil;
using net::test::TestSpdyVisitor;
using net::test::GetSerializedHeaders;

namespace net {

enum SpdyFramerTestTypes {
  SPDY2 = 2,
  SPDY3 = 3,
};

class SpdyFramerTest
    : public ::testing::TestWithParam<SpdyFramerTestTypes> {
 protected:
  virtual void SetUp() {
    spdy_version_ = GetParam();
  }

  void CompareFrame(const string& description,
                    const SpdyFrame& actual_frame,
                    const unsigned char* expected,
                    const int expected_len) {
    const unsigned char* actual =
        reinterpret_cast<const unsigned char*>(actual_frame.data());
    CompareCharArraysWithHexError(
        description, actual, actual_frame.size(), expected, expected_len);
  }

  void CompareFrames(const string& description,
                     const SpdyFrame& expected_frame,
                     const SpdyFrame& actual_frame) {
    CompareCharArraysWithHexError(
        description,
        reinterpret_cast<const unsigned char*>(expected_frame.data()),
        expected_frame.size(),
        reinterpret_cast<const unsigned char*>(actual_frame.data()),
        actual_frame.size());
  }

  // Returns true if the two header blocks have equivalent content.
  bool CompareHeaderBlocks(const SpdyHeaderBlock* expected,
                           const SpdyHeaderBlock* actual) {
    if (expected->size() != actual->size()) {
      LOG(ERROR) << "Expected " << expected->size() << " headers; actually got "
                 << actual->size() << ".";
      return false;
    }
    for (SpdyHeaderBlock::const_iterator it = expected->begin();
         it != expected->end();
         ++it) {
      SpdyHeaderBlock::const_iterator it2 = actual->find(it->first);
      if (it2 == actual->end()) {
        LOG(ERROR) << "Expected header name '" << it->first << "'.";
        return false;
      }
      if (it->second.compare(it2->second) != 0) {
        LOG(ERROR) << "Expected header named '" << it->first
                   << "' to have a value of '" << it->second
                   << "'. The actual value received was '" << it2->second
                   << "'.";
        return false;
      }
    }
    return true;
  }

  void AddSpdySettingFromWireFormat(SettingsMap* settings,
                                    uint32 key,
                                    uint32 value) {
    SettingsFlagsAndId flags_and_id =
        SettingsFlagsAndId::FromWireFormat(spdy_version_, key);
    SpdySettingsIds id = static_cast<SpdySettingsIds>(flags_and_id.id());
    SpdySettingsFlags flags =
        static_cast<SpdySettingsFlags>(flags_and_id.flags());
    CHECK(settings->find(id) == settings->end());
    settings->insert(std::make_pair(id, SettingsFlagsAndValue(flags, value)));
  }

  bool IsSpdy2() { return spdy_version_ == SPDY2; }

  // Version of SPDY protocol to be used.
  unsigned char spdy_version_;
};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyFramerTests,
                        SpdyFramerTest,
                        ::testing::Values(SPDY2, SPDY3));

// Test that we can encode and decode a SpdyHeaderBlock in serialized form.
TEST_P(SpdyFramerTest, HeaderBlockInBuffer) {
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "charlie";
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  // Encode the header block into a SynStream frame.
  scoped_ptr<SpdyFrame> frame(
      framer.CreateSynStream(1,  // stream id
                             0,  // associated stream id
                             1,  // priority
                             0,  // credential slot
                             CONTROL_FLAG_NONE,
                             false,  // compress
                             &headers));
  EXPECT_TRUE(frame.get() != NULL);
  base::StringPiece serialized_headers =
      GetSerializedHeaders(frame.get(), framer);
  SpdyHeaderBlock new_headers;
  EXPECT_TRUE(framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                              serialized_headers.size(),
                                              &new_headers));

  EXPECT_EQ(headers.size(), new_headers.size());
  EXPECT_EQ(headers["alpha"], new_headers["alpha"]);
  EXPECT_EQ(headers["gamma"], new_headers["gamma"]);
}

// Test that if there's not a full frame, we fail to parse it.
TEST_P(SpdyFramerTest, UndersizedHeaderBlockInBuffer) {
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "charlie";
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  // Encode the header block into a SynStream frame.
  scoped_ptr<SpdyFrame> frame(
      framer.CreateSynStream(1,  // stream id
                             0,  // associated stream id
                             1,  // priority
                             0,  // credential slot
                             CONTROL_FLAG_NONE,
                             false,  // compress
                             &headers));
  EXPECT_TRUE(frame.get() != NULL);

  base::StringPiece serialized_headers =
      GetSerializedHeaders(frame.get(), framer);
  SpdyHeaderBlock new_headers;
  EXPECT_FALSE(framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                               serialized_headers.size() - 2,
                                               &new_headers));
}

TEST_P(SpdyFramerTest, OutOfOrderHeaders) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  // Frame builder with plentiful buffer size.
  SpdyFrameBuilder frame(1024);
  frame.WriteControlFrameHeader(framer, SYN_STREAM, CONTROL_FLAG_NONE);

  frame.WriteUInt32(3);  // stream_id
  frame.WriteUInt32(0);  // Associated stream id
  frame.WriteUInt16(0);  // Priority.

  if (IsSpdy2()) {
    frame.WriteUInt16(2);  // Number of headers.
    frame.WriteString("gamma");
    frame.WriteString("gamma");
    frame.WriteString("alpha");
    frame.WriteString("alpha");
  } else {
    frame.WriteUInt32(2);  // Number of headers.
    frame.WriteStringPiece32("gamma");
    frame.WriteStringPiece32("gamma");
    frame.WriteStringPiece32("alpha");
    frame.WriteStringPiece32("alpha");
  }
  // write the length
  frame.RewriteLength(framer);

  SpdyHeaderBlock new_headers;
  scoped_ptr<SpdyFrame> control_frame(frame.take());
  base::StringPiece serialized_headers =
      GetSerializedHeaders(control_frame.get(), framer);
  EXPECT_TRUE(framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                              serialized_headers.size(),
                                              &new_headers));
}

TEST_P(SpdyFramerTest, CreateCredential) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "CREDENTIAL frame";
    const unsigned char kFrameData[] = {
      0x80, spdy_version_, 0x00, 0x0A,
      0x00, 0x00, 0x00, 0x33,
      0x00, 0x03, 0x00, 0x00,
      0x00, 0x05, 'p',  'r',
      'o',  'o',  'f',  0x00,
      0x00, 0x00, 0x06, 'a',
      ' ',  'c',  'e',  'r',
      't',  0x00, 0x00, 0x00,
      0x0C, 'a',  'n',  'o',
      't',  'h',  'e',  'r',
      ' ',  'c',  'e',  'r',
      't',  0x00,  0x00, 0x00,
      0x0A, 'f',  'i',  'n',
      'a',  'l',  ' ',  'c',
      'e',  'r',  't',
    };
    SpdyCredential credential;
    credential.slot = 3;
    credential.proof = "proof";
    credential.certs.push_back("a cert");
    credential.certs.push_back("another cert");
    credential.certs.push_back("final cert");
    scoped_ptr<SpdyFrame> frame(framer.CreateCredentialFrame(credential));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_P(SpdyFramerTest, ParseCredentialFrameData) {
  SpdyFramer framer(spdy_version_);

  {
    unsigned char kFrameData[] = {
      0x80, spdy_version_, 0x00, 0x0A,
      0x00, 0x00, 0x00, 0x33,
      0x00, 0x03, 0x00, 0x00,
      0x00, 0x05, 'p',  'r',
      'o',  'o',  'f',  0x00,
      0x00, 0x00, 0x06, 'a',
      ' ',  'c',  'e',  'r',
      't',  0x00, 0x00, 0x00,
      0x0C, 'a',  'n',  'o',
      't',  'h',  'e',  'r',
      ' ',  'c',  'e',  'r',
      't',  0x00,  0x00, 0x00,
      0x0A, 'f',  'i',  'n',
      'a',  'l',  ' ',  'c',
      'e',  'r',  't',
    };
    SpdyFrame frame(
        reinterpret_cast<char*>(kFrameData), arraysize(kFrameData), false);
    SpdyCredential credential;
    EXPECT_TRUE(SpdyFramer::ParseCredentialData(
        frame.data() + framer.GetControlFrameHeaderSize(),
        frame.size() - framer.GetControlFrameHeaderSize(),
        &credential));
    EXPECT_EQ(3u, credential.slot);
    EXPECT_EQ("proof", credential.proof);
    EXPECT_EQ("a cert", credential.certs.front());
    credential.certs.erase(credential.certs.begin());
    EXPECT_EQ("another cert", credential.certs.front());
    credential.certs.erase(credential.certs.begin());
    EXPECT_EQ("final cert", credential.certs.front());
    credential.certs.erase(credential.certs.begin());
    EXPECT_TRUE(credential.certs.empty());
  }
}

TEST_P(SpdyFramerTest, DuplicateHeader) {
  SpdyFramer framer(spdy_version_);
  // Frame builder with plentiful buffer size.
  SpdyFrameBuilder frame(1024);
  frame.WriteControlFrameHeader(framer, SYN_STREAM, CONTROL_FLAG_NONE);

  frame.WriteUInt32(3);  // stream_id
  frame.WriteUInt32(0);  // associated stream id
  frame.WriteUInt16(0);  // Priority.

  if (IsSpdy2()) {
    frame.WriteUInt16(2);  // Number of headers.
    frame.WriteString("name");
    frame.WriteString("value1");
    frame.WriteString("name");
    frame.WriteString("value2");
  } else {
    frame.WriteUInt32(2);  // Number of headers.
    frame.WriteStringPiece32("name");
    frame.WriteStringPiece32("value1");
    frame.WriteStringPiece32("name");
    frame.WriteStringPiece32("value2");
  }
  // write the length
  frame.RewriteLength(framer);

  SpdyHeaderBlock new_headers;
  framer.set_enable_compression(false);
  scoped_ptr<SpdyFrame> control_frame(frame.take());
  base::StringPiece serialized_headers =
      GetSerializedHeaders(control_frame.get(), framer);
  // This should fail because duplicate headers are verboten by the spec.
  EXPECT_FALSE(framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                               serialized_headers.size(),
                                               &new_headers));
}

TEST_P(SpdyFramerTest, MultiValueHeader) {
  SpdyFramer framer(spdy_version_);
  // Frame builder with plentiful buffer size.
  SpdyFrameBuilder frame(1024);
  frame.WriteControlFrameHeader(framer, SYN_STREAM, CONTROL_FLAG_NONE);

  frame.WriteUInt32(3);  // stream_id
  frame.WriteUInt32(0);  // associated stream id
  frame.WriteUInt16(0);  // Priority.

  string value("value1\0value2");
  if (IsSpdy2()) {
    frame.WriteUInt16(1);  // Number of headers.
    frame.WriteString("name");
    frame.WriteString(value);
  } else {
    frame.WriteUInt32(1);  // Number of headers.
    frame.WriteStringPiece32("name");
    frame.WriteStringPiece32(value);
  }
  // write the length
  frame.RewriteLength(framer);

  SpdyHeaderBlock new_headers;
  framer.set_enable_compression(false);
  scoped_ptr<SpdyFrame> control_frame(frame.take());
  base::StringPiece serialized_headers =
      GetSerializedHeaders(control_frame.get(), framer);
  EXPECT_TRUE(framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                              serialized_headers.size(),
                                              &new_headers));
  EXPECT_TRUE(new_headers.find("name") != new_headers.end());
  EXPECT_EQ(value, new_headers.find("name")->second);
}

TEST_P(SpdyFramerTest, BasicCompression) {
  SpdyHeaderBlock headers;
  headers["server"] = "SpdyServer 1.0";
  headers["date"] = "Mon 12 Jan 2009 12:12:12 PST";
  headers["status"] = "200";
  headers["version"] = "HTTP/1.1";
  headers["content-type"] = "text/html";
  headers["content-length"] = "12";

  scoped_ptr<TestSpdyVisitor> visitor(new TestSpdyVisitor(spdy_version_));
  SpdyFramer framer(spdy_version_);
  framer.set_debug_visitor(visitor.get());
  scoped_ptr<SpdyFrame> frame1(
      framer.CreateSynStream(1,  // stream id
                             0,  // associated stream id
                             1,  // priority
                             0,  // credential slot
                             CONTROL_FLAG_NONE,
                             true,  // compress
                             &headers));
  if (IsSpdy2()) {
    EXPECT_EQ(139u, visitor->last_uncompressed_size_);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(93u, visitor->last_compressed_size_);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(135u, visitor->last_compressed_size_);
#endif  // !defined(USE_SYSTEM_ZLIB)
  } else {
    EXPECT_EQ(165u, visitor->last_uncompressed_size_);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(72u, visitor->last_compressed_size_);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(117u, visitor->last_compressed_size_);
#endif  // !defined(USE_SYSTEM_ZLIB)
  }
  scoped_ptr<SpdyFrame> frame2(
      framer.CreateSynStream(1,  // stream id
                             0,  // associated stream id
                             1,  // priority
                             0,  // credential slot
                             CONTROL_FLAG_NONE,
                             true,  // compress
                             &headers));

  // Expect the second frame to be more compact than the first.
  EXPECT_LE(frame2->size(), frame1->size());

  // Decompress the first frame
  scoped_ptr<SpdyFrame> frame3(SpdyFramerTestUtil::DecompressFrame(
      &framer, *frame1.get()));

  // Decompress the second frame
  visitor.reset(new TestSpdyVisitor(spdy_version_));
  framer.set_debug_visitor(visitor.get());
  scoped_ptr<SpdyFrame> frame4(SpdyFramerTestUtil::DecompressFrame(
      &framer, *frame2.get()));
  if (IsSpdy2()) {
    EXPECT_EQ(139u, visitor->last_uncompressed_size_);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(9u, visitor->last_compressed_size_);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(101u, visitor->last_compressed_size_);
#endif  // !defined(USE_SYSTEM_ZLIB)
  } else {
    EXPECT_EQ(165u, visitor->last_uncompressed_size_);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(9u, visitor->last_compressed_size_);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(102u, visitor->last_compressed_size_);
#endif  // !defined(USE_SYSTEM_ZLIB)
  }

  // Expect frames 3 & 4 to be the same.
  CompareFrames("Uncompressed SYN_STREAM", *frame3, *frame4);

  // Expect frames 3 to be the same as a uncompressed frame created
  // from scratch.
  framer.set_enable_compression(false);
  scoped_ptr<SpdyFrame> uncompressed_frame(
      framer.CreateSynStream(1,  // stream id
                             0,  // associated stream id
                             1,  // priority
                             0,  // credential slot
                             CONTROL_FLAG_NONE,
                             false,  // compress
                             &headers));
  CompareFrames("Uncompressed SYN_STREAM", *frame3, *uncompressed_frame);
}

TEST_P(SpdyFramerTest, CompressEmptyHeaders) {
  // See crbug.com/172383
  SpdyHeaderBlock headers;
  headers["server"] = "SpdyServer 1.0";
  headers["date"] = "Mon 12 Jan 2009 12:12:12 PST";
  headers["status"] = "200";
  headers["version"] = "HTTP/1.1";
  headers["content-type"] = "text/html";
  headers["content-length"] = "12";
  headers["x-empty-header"] = "";

  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);
  scoped_ptr<SpdyFrame> frame1(
      framer.CreateSynStream(1,  // stream id
                             0,  // associated stream id
                             1,  // priority
                             0,  // credential slot
                             CONTROL_FLAG_NONE,
                             true,  // compress
                             &headers));
}

TEST_P(SpdyFramerTest, Basic) {
  const unsigned char kV2Input[] = {
    0x80, spdy_version_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, spdy_version_, 0x00, 0x08,  // HEADERS on Stream #1
    0x00, 0x00, 0x00, 0x18,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x02, 'h', '2',
    0x00, 0x02, 'v', '2',
    0x00, 0x02, 'h', '3',
    0x00, 0x02, 'v', '3',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_, 0x00, 0x01,  // SYN Stream #3
    0x00, 0x00, 0x00, 0x0c,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_, 0x00, 0x03,  // RST_STREAM on Stream #1
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x00,

    0x80, spdy_version_, 0x00, 0x03,  // RST_STREAM on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
  };

  const unsigned char kV3Input[] = {
    0x80, spdy_version_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x1a,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x02, 'h', 'h',
    0x00, 0x00, 0x00, 0x02,
    'v', 'v',

    0x80, spdy_version_, 0x00, 0x08,  // HEADERS on Stream #1
    0x00, 0x00, 0x00, 0x20,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x02,
    'h', '2',
    0x00, 0x00, 0x00, 0x02,
    'v', '2', 0x00, 0x00,
    0x00, 0x02, 'h', '3',
    0x00, 0x00, 0x00, 0x02,
    'v', '3',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_, 0x00, 0x01,  // SYN Stream #3
    0x00, 0x00, 0x00, 0x0e,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_, 0x00, 0x03,  // RST_STREAM on Stream #1
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x00,

    0x80, spdy_version_, 0x00, 0x03,  // RST_STREAM on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
  };

  TestSpdyVisitor visitor(spdy_version_);
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2Input, sizeof(kV2Input));
  } else {
    visitor.SimulateInFramer(kV3Input, sizeof(kV3Input));
  }

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(24, visitor.data_bytes_);
  EXPECT_EQ(2, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(4, visitor.data_frame_count_);
}

// Test that the FIN flag on a data frame signifies EOF.
TEST_P(SpdyFramerTest, FinOnDataFrame) {
  const unsigned char kV2Input[] = {
    0x80, spdy_version_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, spdy_version_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x00, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'a', 'a',
    0x00, 0x02, 'b', 'b',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1, with EOF
    0x01, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,
  };
  const unsigned char kV3Input[] = {
    0x80, spdy_version_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x1a,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x02, 'h', 'h',
    0x00, 0x00, 0x00, 0x02,
    'v', 'v',

    0x80, spdy_version_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    'a', 'a',   0x00, 0x00,
    0x00, 0x02, 'b', 'b',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1, with EOF
    0x01, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,
  };

  TestSpdyVisitor visitor(spdy_version_);
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2Input, sizeof(kV2Input));
  } else {
    visitor.SimulateInFramer(kV3Input, sizeof(kV3Input));
  }

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(1, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(16, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(2, visitor.data_frame_count_);
}

// Test that the FIN flag on a SYN reply frame signifies EOF.
TEST_P(SpdyFramerTest, FinOnSynReplyFrame) {
  const unsigned char kV2Input[] = {
    0x80, spdy_version_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, spdy_version_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x01, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'a', 'a',
    0x00, 0x02, 'b', 'b',
  };
  const unsigned char kV3Input[] = {
    0x80, spdy_version_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x1a,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x02, 'h', 'h',
    0x00, 0x00, 0x00, 0x02,
    'v', 'v',

    0x80, spdy_version_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x01, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    'a', 'a',   0x00, 0x00,
    0x00, 0x02, 'b', 'b',
  };

  TestSpdyVisitor visitor(spdy_version_);
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2Input, sizeof(kV2Input));
  } else {
    visitor.SimulateInFramer(kV3Input, sizeof(kV3Input));
  }

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(1, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(1, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(0, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, HeaderCompression) {
  SpdyFramer send_framer(spdy_version_);
  SpdyFramer recv_framer(spdy_version_);

  send_framer.set_enable_compression(true);
  recv_framer.set_enable_compression(true);

  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kHeader3[] = "header3";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";
  const char kValue3[] = "value3";

  // SYN_STREAM #1
  SpdyHeaderBlock block;
  block[kHeader1] = kValue1;
  block[kHeader2] = kValue2;
  SpdyControlFlags flags(CONTROL_FLAG_NONE);
  SpdySynStreamIR syn_ir_1(1);
  syn_ir_1.SetHeader(kHeader1, kValue1);
  syn_ir_1.SetHeader(kHeader2, kValue2);
  scoped_ptr<SpdyFrame> syn_frame_1(send_framer.SerializeSynStream(syn_ir_1));
  EXPECT_TRUE(syn_frame_1.get() != NULL);

  // SYN_STREAM #2
  block[kHeader3] = kValue3;
  scoped_ptr<SpdyFrame> syn_frame_2(
      send_framer.CreateSynStream(3,  // stream id
                                  0,  // associated stream id
                                  0,  // priority
                                  0,  // credential slot
                                  flags,
                                  true,  // compress
                                  &block));
  EXPECT_TRUE(syn_frame_2.get() != NULL);

  // Now start decompressing
  scoped_ptr<SpdyFrame> decompressed;
  scoped_ptr<SpdyFrame> uncompressed;
  base::StringPiece serialized_headers;
  SpdyHeaderBlock decompressed_headers;

  // Decompress SYN_STREAM #1
  decompressed.reset(SpdyFramerTestUtil::DecompressFrame(
      &recv_framer, *syn_frame_1.get()));
  EXPECT_TRUE(decompressed.get() != NULL);
  serialized_headers = GetSerializedHeaders(decompressed.get(), send_framer);
  EXPECT_TRUE(recv_framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                                   serialized_headers.size(),
                                                   &decompressed_headers));
  EXPECT_EQ(2u, decompressed_headers.size());
  EXPECT_EQ(kValue1, decompressed_headers[kHeader1]);
  EXPECT_EQ(kValue2, decompressed_headers[kHeader2]);

  // Decompress SYN_STREAM #2
  decompressed.reset(SpdyFramerTestUtil::DecompressFrame(
      &recv_framer, *syn_frame_2.get()));
  EXPECT_TRUE(decompressed.get() != NULL);
  serialized_headers = GetSerializedHeaders(decompressed.get(), send_framer);
  decompressed_headers.clear();
  EXPECT_TRUE(recv_framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                                   serialized_headers.size(),
                                                   &decompressed_headers));
  EXPECT_EQ(3u, decompressed_headers.size());
  EXPECT_EQ(kValue1, decompressed_headers[kHeader1]);
  EXPECT_EQ(kValue2, decompressed_headers[kHeader2]);
  EXPECT_EQ(kValue3, decompressed_headers[kHeader3]);
}

// Verify we don't leak when we leave streams unclosed
TEST_P(SpdyFramerTest, UnclosedStreamDataCompressors) {
  SpdyFramer send_framer(spdy_version_);

  send_framer.set_enable_compression(true);

  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";

  SpdyHeaderBlock block;
  block[kHeader1] = kValue1;
  block[kHeader2] = kValue2;
  SpdyControlFlags flags(CONTROL_FLAG_NONE);
  scoped_ptr<SpdyFrame> syn_frame(
      send_framer.CreateSynStream(1,  // stream id
                                  0,  // associated stream id
                                  0,  // priority
                                  0,  // credential slot
                                  flags,
                                  true,  // compress
                                  &block));
  EXPECT_TRUE(syn_frame.get() != NULL);

  const char bytes[] = "this is a test test test test test!";
  scoped_ptr<SpdyFrame> send_frame(
      send_framer.CreateDataFrame(
          1, bytes, arraysize(bytes),
          static_cast<SpdyDataFlags>(DATA_FLAG_FIN)));
  EXPECT_TRUE(send_frame.get() != NULL);

  // Run the inputs through the framer.
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  const unsigned char* data;
  data = reinterpret_cast<const unsigned char*>(syn_frame->data());
  visitor.SimulateInFramer(data, syn_frame->size());
  data = reinterpret_cast<const unsigned char*>(send_frame->data());
  visitor.SimulateInFramer(data, send_frame->size());

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(arraysize(bytes), static_cast<unsigned>(visitor.data_bytes_));
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(1, visitor.data_frame_count_);
}

// Verify we can decompress the stream even if handed over to the
// framer 1 byte at a time.
TEST_P(SpdyFramerTest, UnclosedStreamDataCompressorsOneByteAtATime) {
  SpdyFramer send_framer(spdy_version_);

  send_framer.set_enable_compression(true);

  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";

  SpdyHeaderBlock block;
  block[kHeader1] = kValue1;
  block[kHeader2] = kValue2;
  SpdyControlFlags flags(CONTROL_FLAG_NONE);
  scoped_ptr<SpdyFrame> syn_frame(
      send_framer.CreateSynStream(1,  // stream id
                                  0,  // associated stream id
                                  0,  // priority
                                  0,  // credential slot
                                  flags,
                                  true,  // compress
                                  &block));
  EXPECT_TRUE(syn_frame.get() != NULL);

  const char bytes[] = "this is a test test test test test!";
  scoped_ptr<SpdyFrame> send_frame(
      send_framer.CreateDataFrame(
          1, bytes, arraysize(bytes),
          static_cast<SpdyDataFlags>(DATA_FLAG_FIN)));
  EXPECT_TRUE(send_frame.get() != NULL);

  // Run the inputs through the framer.
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  const unsigned char* data;
  data = reinterpret_cast<const unsigned char*>(syn_frame->data());
  for (size_t idx = 0; idx < syn_frame->size(); ++idx) {
    visitor.SimulateInFramer(data + idx, 1);
    ASSERT_EQ(0, visitor.error_count_);
  }
  data = reinterpret_cast<const unsigned char*>(send_frame->data());
  for (size_t idx = 0; idx < send_frame->size(); ++idx) {
    visitor.SimulateInFramer(data + idx, 1);
    ASSERT_EQ(0, visitor.error_count_);
  }

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(arraysize(bytes), static_cast<unsigned>(visitor.data_bytes_));
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(1, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, WindowUpdateFrame) {
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> window_update_frame(
      framer.CreateWindowUpdate(1, 0x12345678));

  const char kDescription[] = "WINDOW_UPDATE frame, stream 1, delta 0x12345678";
  const unsigned char expected_data_frame[] = {
      0x80, spdy_version_, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x12, 0x34, 0x56, 0x78
  };

  CompareFrame(kDescription, *window_update_frame, expected_data_frame,
               arraysize(expected_data_frame));
}

TEST_P(SpdyFramerTest, CreateDataFrame) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "'hello' data frame, no FIN";
    const unsigned char kFrameData[] = {
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x05,
      'h', 'e', 'l', 'l',
      'o'
    };
    const char bytes[] = "hello";

    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        1, bytes, strlen(bytes), DATA_FLAG_NONE));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));

    SpdyDataIR data_ir(1);
    data_ir.SetDataShallow(base::StringPiece(bytes, strlen(bytes)));
    frame.reset(framer.SerializeDataFrameHeader(data_ir));
    CompareCharArraysWithHexError(
        kDescription,
        reinterpret_cast<const unsigned char*>(frame->data()),
        framer.GetDataFrameMinimumSize(),
        kFrameData,
        framer.GetDataFrameMinimumSize());
  }

  {
    const char kDescription[] = "Data frame with negative data byte, no FIN";
    const unsigned char kFrameData[] = {
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
      0xff
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        1, "\xff", 1, DATA_FLAG_NONE));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "'hello' data frame, with FIN";
    const unsigned char kFrameData[] = {
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x05,
      'h', 'e', 'l', 'l',
      'o'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        1, "hello", 5, DATA_FLAG_FIN));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "Empty data frame";
    const unsigned char kFrameData[] = {
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        1, "", 0, DATA_FLAG_NONE));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "Data frame with max stream ID";
    const unsigned char kFrameData[] = {
      0x7f, 0xff, 0xff, 0xff,
      0x01, 0x00, 0x00, 0x05,
      'h', 'e', 'l', 'l',
      'o'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        0x7fffffff, "hello", 5, DATA_FLAG_FIN));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "Large data frame";
    const int kDataSize = 4 * 1024 * 1024;  // 4 MB
    const string kData(kDataSize, 'A');
    const unsigned char kFrameHeader[] = {
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x40, 0x00, 0x00,
    };

    const int kFrameSize = arraysize(kFrameHeader) + kDataSize;
    scoped_array<unsigned char> expected_frame_data(
        new unsigned char[kFrameSize]);
    memcpy(expected_frame_data.get(), kFrameHeader, arraysize(kFrameHeader));
    memset(expected_frame_data.get() + arraysize(kFrameHeader), 'A', kDataSize);

    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        1, kData.data(), kData.size(), DATA_FLAG_FIN));
    CompareFrame(kDescription, *frame, expected_frame_data.get(), kFrameSize);
  }
}

TEST_P(SpdyFramerTest, CreateSynStreamUncompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "SYN_STREAM frame, lowest pri, slot 2, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const unsigned char kPri = IsSpdy2() ? 0xC0 : 0xE0;
    const unsigned char kCre = IsSpdy2() ? 0 : 2;
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x20,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      kPri, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x2a,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      kPri, kCre, 0x00, 0x00,
      0x00, 0x02, 0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x00, 0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'b',
      'a',  'r'
    };
    scoped_ptr<SpdyFrame> frame(
        framer.CreateSynStream(1,  // stream id
                               0,  // associated stream id
                               framer.GetLowestPriority(),
                               kCre,  // credential slot
                               CONTROL_FLAG_NONE,
                               false,  // compress
                               &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }

  {
    const char kDescription[] =
        "SYN_STREAM frame with a 0-length header name, highest pri, FIN, "
        "max stream ID";

    SpdyHeaderBlock headers;
    headers[""] = "foo";
    headers["foo"] = "bar";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x1D,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x27,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x02, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r'
    };
    scoped_ptr<SpdyFrame> frame(
        framer.CreateSynStream(0x7fffffff,  // stream id
                               0x7fffffff,  // associated stream id
                               framer.GetHighestPriority(),
                               0,  // credential slot
                               CONTROL_FLAG_FIN,
                               false,  // compress
                               &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }

  {
    const char kDescription[] =
        "SYN_STREAM frame with a 0-length header val, high pri, FIN, "
        "max stream ID";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "";

    const unsigned char kPri = IsSpdy2() ? 0x40 : 0x20;
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x1D,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      kPri, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x27,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      kPri, 0x00, 0x00, 0x00,
      0x00, 0x02, 0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x00, 0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x00
    };
    scoped_ptr<SpdyFrame> frame(
        framer.CreateSynStream(0x7fffffff,  // stream id
                               0x7fffffff,  // associated stream id
                               1,  // priority
                               0,  // credential slot
                               CONTROL_FLAG_FIN,
                               false,  // compress
                               &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }
}

// TODO(phajdan.jr): Clean up after we no longer need
// to workaround http://crbug.com/139744.
#if !defined(USE_SYSTEM_ZLIB)
TEST_P(SpdyFramerTest, CreateSynStreamCompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);

  {
    const char kDescription[] =
        "SYN_STREAM frame, low pri, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const SpdyPriority priority = IsSpdy2() ? 2 : 4;
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x36,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      0x80, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x06, 0x08, 0xa0,
      0xb4, 0xfc, 0x7c, 0x80,
      0x00, 0x62, 0x60, 0x4e,
      0xcb, 0xcf, 0x67, 0x60,
      0x06, 0x08, 0xa0, 0xa4,
      0xc4, 0x22, 0x80, 0x00,
      0x02, 0x00, 0x00, 0x00,
      0xff, 0xff,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x37,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      0x80, 0x00, 0x38, 0xEA,
      0xE3, 0xC6, 0xA7, 0xC2,
      0x02, 0xE5, 0x0E, 0x50,
      0xC2, 0x4B, 0x4A, 0x04,
      0xE5, 0x0B, 0x66, 0x80,
      0x00, 0x4A, 0xCB, 0xCF,
      0x07, 0x08, 0x20, 0x10,
      0x95, 0x96, 0x9F, 0x0F,
      0xA2, 0x00, 0x02, 0x28,
      0x29, 0xB1, 0x08, 0x20,
      0x80, 0x00, 0x00, 0x00,
      0x00, 0xFF, 0xFF,
    };
    scoped_ptr<SpdyFrame> frame(
        framer.CreateSynStream(1,  // stream id
                               0,  // associated stream id
                               priority,
                               0,  // credential slot
                               CONTROL_FLAG_NONE,
                               true,  // compress
                               &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }
}
#endif  // !defined(USE_SYSTEM_ZLIB)

TEST_P(SpdyFramerTest, CreateSynReplyUncompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "SYN_REPLY frame, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x1C,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x24,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'b',  'a',  'r'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynReply(
        1, CONTROL_FLAG_NONE, false, &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }

  {
    const char kDescription[] =
        "SYN_REPLY frame with a 0-length header name, FIN, max stream ID";

    SpdyHeaderBlock headers;
    headers[""] = "foo";
    headers["foo"] = "bar";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynReply(
        0x7fffffff, CONTROL_FLAG_FIN, false, &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }

  {
    const char kDescription[] =
        "SYN_REPLY frame with a 0-length header val, FIN, max stream ID";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x00
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynReply(
        0x7fffffff, CONTROL_FLAG_FIN, false, &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }
}

// TODO(phajdan.jr): Clean up after we no longer need
// to workaround http://crbug.com/139744.
#if !defined(USE_SYSTEM_ZLIB)
TEST_P(SpdyFramerTest, CreateSynReplyCompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);

  {
    const char kDescription[] = "SYN_REPLY frame, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x32,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x06, 0x08, 0xa0,
      0xb4, 0xfc, 0x7c, 0x80,
      0x00, 0x62, 0x60, 0x4e,
      0xcb, 0xcf, 0x67, 0x60,
      0x06, 0x08, 0xa0, 0xa4,
      0xc4, 0x22, 0x80, 0x00,
      0x02, 0x00, 0x00, 0x00,
      0xff, 0xff,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x31,
      0x00, 0x00, 0x00, 0x01,
      0x38, 0xea, 0xe3, 0xc6,
      0xa7, 0xc2, 0x02, 0xe5,
      0x0e, 0x50, 0xc2, 0x4b,
      0x4a, 0x04, 0xe5, 0x0b,
      0x66, 0x80, 0x00, 0x4a,
      0xcb, 0xcf, 0x07, 0x08,
      0x20, 0x10, 0x95, 0x96,
      0x9f, 0x0f, 0xa2, 0x00,
      0x02, 0x28, 0x29, 0xb1,
      0x08, 0x20, 0x80, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynReply(
        1, CONTROL_FLAG_NONE, true, &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }
}
#endif  // !defined(USE_SYSTEM_ZLIB)

TEST_P(SpdyFramerTest, CreateRstStream) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "RST_STREAM frame";
    const unsigned char kFrameData[] = {
      0x80, spdy_version_, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(
        framer.CreateRstStream(1, RST_STREAM_PROTOCOL_ERROR));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "RST_STREAM frame with max stream ID";
    const unsigned char kFrameData[] = {
      0x80, spdy_version_, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateRstStream(
        0x7FFFFFFF, RST_STREAM_PROTOCOL_ERROR));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "RST_STREAM frame with max status code";
    const unsigned char kFrameData[] = {
      0x80, spdy_version_, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x06,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateRstStream(
        0x7FFFFFFF, RST_STREAM_INTERNAL_ERROR));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_P(SpdyFramerTest, CreateSettings) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "Network byte order SETTINGS frame";

    uint32 kValue = 0x0a0b0c0d;
    SpdySettingsFlags kFlags = static_cast<SpdySettingsFlags>(0x01);
    SpdySettingsIds kId = static_cast<SpdySettingsIds>(0x020304);

    SettingsMap settings;
    settings[kId] = SettingsFlagsAndValue(kFlags, kValue);

    EXPECT_EQ(kFlags, settings[kId].first);
    EXPECT_EQ(kValue, settings[kId].second);

    const unsigned char kFrameDatav2[] = {
      0x80, spdy_version_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x0c,
      0x00, 0x00, 0x00, 0x01,
      0x04, 0x03, 0x02, 0x01,
      0x0a, 0x0b, 0x0c, 0x0d,
    };

    const unsigned char kFrameDatav3[] = {
      0x80, spdy_version_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x0c,
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x02, 0x03, 0x04,
      0x0a, 0x0b, 0x0c, 0x0d,
    };

    scoped_ptr<SpdyFrame> frame(framer.CreateSettings(settings));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kFrameDatav2 : kFrameDatav3,
                 arraysize(kFrameDatav3));  // Size is unchanged among versions.
  }

  {
    const char kDescription[] = "Basic SETTINGS frame";

    SettingsMap settings;
    AddSpdySettingFromWireFormat(
        &settings, 0x00000000, 0x00000001);  // 1st Setting
    AddSpdySettingFromWireFormat(
        &settings, 0x01000001, 0x00000002);  // 2nd Setting
    AddSpdySettingFromWireFormat(
        &settings, 0x02000002, 0x00000003);  // 3rd Setting
    AddSpdySettingFromWireFormat(
        &settings, 0x03000003, 0xff000004);  // 4th Setting

    const unsigned char kFrameData[] = {
      0x80, spdy_version_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x24,
      0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00,  // 1st Setting
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x01,  // 2nd Setting
      0x00, 0x00, 0x00, 0x02,
      0x02, 0x00, 0x00, 0x02,  // 3rd Setting
      0x00, 0x00, 0x00, 0x03,
      0x03, 0x00, 0x00, 0x03,  // 4th Setting
      0xff, 0x00, 0x00, 0x04,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSettings(settings));
    CompareFrame(kDescription,
                 *frame,
                 kFrameData,
                 arraysize(kFrameData));
  }

  {
    const char kDescription[] = "Empty SETTINGS frame";

    SettingsMap settings;

    const unsigned char kFrameData[] = {
      0x80, spdy_version_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSettings(settings));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_P(SpdyFramerTest, CreatePingFrame) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "PING frame";
    const unsigned char kFrameData[] = {
        0x80, spdy_version_, 0x00, 0x06,
        0x00, 0x00, 0x00, 0x04,
        0x12, 0x34, 0x56, 0x78,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreatePingFrame(0x12345678u));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_P(SpdyFramerTest, CreateGoAway) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "GOAWAY frame";
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateGoAway(0, GOAWAY_OK));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }

  {
    const char kDescription[] = "GOAWAY frame with max stream ID, status";
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x04,
      0x7f, 0xff, 0xff, 0xff,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateGoAway(0x7FFFFFFF,
                                                    GOAWAY_INTERNAL_ERROR));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }
}

TEST_P(SpdyFramerTest, CreateHeadersUncompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "HEADERS frame, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x1C,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x24,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'b',  'a',  'r'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateHeaders(
        1, CONTROL_FLAG_NONE, false, &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header name, FIN, max stream ID";

    SpdyHeaderBlock headers;
    headers[""] = "foo";
    headers["foo"] = "bar";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateHeaders(
        0x7fffffff, CONTROL_FLAG_FIN, false, &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header val, FIN, max stream ID";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x00
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateHeaders(
        0x7fffffff, CONTROL_FLAG_FIN, false, &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }
}

// TODO(phajdan.jr): Clean up after we no longer need
// to workaround http://crbug.com/139744.
#if !defined(USE_SYSTEM_ZLIB)
TEST_P(SpdyFramerTest, CreateHeadersCompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);

  {
    const char kDescription[] = "HEADERS frame, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x32,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x06, 0x08, 0xa0,
      0xb4, 0xfc, 0x7c, 0x80,
      0x00, 0x62, 0x60, 0x4e,
      0xcb, 0xcf, 0x67, 0x60,
      0x06, 0x08, 0xa0, 0xa4,
      0xc4, 0x22, 0x80, 0x00,
      0x02, 0x00, 0x00, 0x00,
      0xff, 0xff,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x31,
      0x00, 0x00, 0x00, 0x01,
      0x38, 0xea, 0xe3, 0xc6,
      0xa7, 0xc2, 0x02, 0xe5,
      0x0e, 0x50, 0xc2, 0x4b,
      0x4a, 0x04, 0xe5, 0x0b,
      0x66, 0x80, 0x00, 0x4a,
      0xcb, 0xcf, 0x07, 0x08,
      0x20, 0x10, 0x95, 0x96,
      0x9f, 0x0f, 0xa2, 0x00,
      0x02, 0x28, 0x29, 0xb1,
      0x08, 0x20, 0x80, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateHeaders(
        1, CONTROL_FLAG_NONE, true, &headers));
    CompareFrame(kDescription,
                 *frame,
                 IsSpdy2() ? kV2FrameData : kV3FrameData,
                 IsSpdy2() ? arraysize(kV2FrameData) : arraysize(kV3FrameData));
  }
}
#endif  // !defined(USE_SYSTEM_ZLIB)

TEST_P(SpdyFramerTest, CreateWindowUpdate) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "WINDOW_UPDATE frame";
    const unsigned char kFrameData[] = {
      0x80, spdy_version_, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(
        framer.CreateWindowUpdate(1, 1));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "WINDOW_UPDATE frame with max stream ID";
    const unsigned char kFrameData[] = {
      0x80, spdy_version_, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateWindowUpdate(0x7FFFFFFF, 1));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "WINDOW_UPDATE frame with max window delta";
    const unsigned char kFrameData[] = {
      0x80, spdy_version_, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x7f, 0xff, 0xff, 0xff,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateWindowUpdate(1, 0x7FFFFFFF));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_P(SpdyFramerTest, ReadCompressedSynStreamHeaderBlock) {
  SpdyHeaderBlock headers;
  headers["aa"] = "vv";
  headers["bb"] = "ww";
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateSynStream(1,                     // stream_id
                             0,                     // associated_stream_id
                             1,                     // priority
                             0,                     // credential_slot
                             CONTROL_FLAG_NONE,
                             true,                  // compress
                             &headers));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ReadCompressedSynReplyHeaderBlock) {
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "delta";
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateSynReply(1,                     // stream_id
                            CONTROL_FLAG_NONE,
                            true,                  // compress
                            &headers));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.syn_reply_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ReadCompressedHeadersHeaderBlock) {
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "delta";
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateHeaders(1,                     // stream_id
                           CONTROL_FLAG_NONE,
                           true,                  // compress
                           &headers));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  // control_frame_header_data_count_ depends on the random sequence
  // produced by rand(), so adding, removing or running single tests
  // alters this value.  The best we can do is assert that it happens
  // at least twice.
  EXPECT_LE(2, visitor.control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ReadCompressedHeadersHeaderBlockWithHalfClose) {
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "delta";
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateHeaders(1,                     // stream_id
                           CONTROL_FLAG_FIN,
                           true,                  // compress
                           &headers));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  // control_frame_header_data_count_ depends on the random sequence
  // produced by rand(), so adding, removing or running single tests
  // alters this value.  The best we can do is assert that it happens
  // at least twice.
  EXPECT_LE(2, visitor.control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ControlFrameAtMaxSizeLimit) {
  SpdyHeaderBlock headers;
  // Size a header value to just fit inside the control frame buffer:
  //                               SPDY 2             SPDY 3
  //   SYN_STREAM header:          18 bytes           18 bytes
  //   Serialized header block:
  //     # headers                  2 bytes (uint16)   4 bytes (uint32)
  //     name length                2 bytes (uint16)   4 bytes (uint32)
  //     name text ("aa")           2 bytes            2 bytes
  //     value length               2 bytes (uint16)   4 bytes (uint32)
  //                              ---                ---
  //                               26 bytes           32 bytes
  const size_t overhead = IsSpdy2() ? 26 : 32;
  TestSpdyVisitor visitor(spdy_version_);
  const size_t big_value_size =
      visitor.control_frame_buffer_max_size() - overhead;
  std::string big_value(big_value_size, 'x');
  headers["aa"] = big_value.c_str();
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateSynStream(1,                     // stream_id
                             0,                     // associated_stream_id
                             1,                     // priority
                             0,                     // credential_slot
                             CONTROL_FLAG_NONE,
                             false,                 // compress
                             &headers));
  EXPECT_TRUE(control_frame.get() != NULL);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
  EXPECT_LT(big_value_size, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, ControlFrameTooLarge) {
  SpdyHeaderBlock headers;
  // See size calculation for test above. This is one byte larger, which
  // should exceed the control frame buffer capacity by that one byte.
  const size_t overhead = IsSpdy2() ? 25 : 31;
  TestSpdyVisitor visitor(spdy_version_);
  const size_t kBigValueSize =
      visitor.control_frame_buffer_max_size() - overhead;
  std::string big_value(kBigValueSize, 'x');
  headers["aa"] = big_value.c_str();
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateSynStream(1,                     // stream_id
                             0,                     // associated_stream_id
                             1,                     // priority
                             0,                     // credential_slot
                             CONTROL_FLAG_NONE,
                             false,                 // compress
                             &headers));
  EXPECT_TRUE(control_frame.get() != NULL);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_FALSE(visitor.header_buffer_valid_);
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_CONTROL_PAYLOAD_TOO_LARGE,
            visitor.framer_.error_code());
  EXPECT_EQ(0, visitor.syn_frame_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

// Check that the framer stops delivering header data chunks once the visitor
// declares it doesn't want any more. This is important to guard against
// "zip bomb" types of attacks.
TEST_P(SpdyFramerTest, ControlFrameMuchTooLarge) {
  SpdyHeaderBlock headers;
  const size_t kHeaderBufferChunks = 4;
  const size_t kHeaderBufferSize =
      TestSpdyVisitor::header_data_chunk_max_size() * kHeaderBufferChunks;
  const size_t big_value_size = kHeaderBufferSize * 2;
  std::string big_value(big_value_size, 'x');
  headers["aa"] = big_value.c_str();
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateSynStream(1,                     // stream_id
                             0,                     // associated_stream_id
                             1,                     // priority
                             0,                     // credential_slot
                             CONTROL_FLAG_FIN,      // half close
                             true,                  // compress
                             &headers));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.set_header_buffer_size(kHeaderBufferSize);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_FALSE(visitor.header_buffer_valid_);
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_CONTROL_PAYLOAD_TOO_LARGE,
            visitor.framer_.error_code());

  // The framer should have stoped delivering chunks after the visitor
  // signaled "stop" by returning false from OnControlFrameHeaderData().
  //
  // control_frame_header_data_count_ depends on the random sequence
  // produced by rand(), so adding, removing or running single tests
  // alters this value.  The best we can do is assert that it happens
  // at least kHeaderBufferChunks + 1.
  EXPECT_LE(kHeaderBufferChunks + 1,
            static_cast<unsigned>(visitor.control_frame_header_data_count_));
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);

  // The framer should not have sent half-close to the visitor.
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
}

TEST_P(SpdyFramerTest, DecompressCorruptHeaderBlock) {
  SpdyHeaderBlock headers;
  headers["aa"] = "alpha beta gamma delta";
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  // Construct a SYN_STREAM control frame without compressing the header block,
  // and have the framer try to decompress it. This will cause the framer to
  // deal with a decompression error.
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateSynStream(1,                     // stream_id
                             0,                     // associated_stream_id
                             1,                     // priority
                             0,                     // credential_slot
                             CONTROL_FLAG_NONE,
                             false,                 // compress
                             &headers));
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_DECOMPRESS_FAILURE, visitor.framer_.error_code());
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, ControlFrameSizesAreValidated) {
  // Create a GoAway frame that has a few extra bytes at the end.
  // We create enough overhead to overflow the framer's control frame buffer.
  ASSERT_GE(254u, SpdyFramer::kControlFrameBufferSize);
  const unsigned char length =  1 + SpdyFramer::kControlFrameBufferSize;
  const unsigned char kV2FrameData[] = {
    0x80, spdy_version_, 0x00, 0x07,
    0x00, 0x00, 0x00, length,
    0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char kV3FrameData[] = {
    0x80, spdy_version_, 0x00, 0x07,
    0x00, 0x00, 0x00, length,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  };
  SpdyFramer framer(spdy_version_);
  const size_t pad_length =
      length + framer.GetControlFrameHeaderSize() -
      (IsSpdy2() ? sizeof(kV2FrameData) : sizeof(kV3FrameData));
  string pad('A', pad_length);
  TestSpdyVisitor visitor(spdy_version_);

  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2FrameData, sizeof(kV2FrameData));
  } else {
    visitor.SimulateInFramer(kV3FrameData, sizeof(kV3FrameData));
  }
  visitor.SimulateInFramer(
      reinterpret_cast<const unsigned char*>(pad.c_str()),
      pad.length());

  EXPECT_EQ(1, visitor.error_count_);  // This generated an error.
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME,
            visitor.framer_.error_code());
  EXPECT_EQ(0, visitor.goaway_count_);  // Frame not parsed.
}

TEST_P(SpdyFramerTest, ReadZeroLenSettingsFrame) {
  SpdyFramer framer(spdy_version_);
  SettingsMap settings;
  scoped_ptr<SpdyFrame> control_frame(framer.CreateSettings(settings));
  SetFrameLength(control_frame.get(), 0, spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      framer.GetControlFrameHeaderSize());
  // Should generate an error, since zero-len settings frames are unsupported.
  EXPECT_EQ(1, visitor.error_count_);
}

// Tests handling of SETTINGS frames with invalid length.
TEST_P(SpdyFramerTest, ReadBogusLenSettingsFrame) {
  SpdyFramer framer(spdy_version_);
  SettingsMap settings;
  // Add a setting to pad the frame so that we don't get a buffer overflow when
  // calling SimulateInFramer() below.
  settings[SETTINGS_UPLOAD_BANDWIDTH] =
      SettingsFlagsAndValue(SETTINGS_FLAG_PLEASE_PERSIST, 0x00000002);
  scoped_ptr<SpdyFrame> control_frame(framer.CreateSettings(settings));
  const size_t kNewLength = 5;
  SetFrameLength(control_frame.get(), kNewLength, spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      framer.GetControlFrameHeaderSize() + kNewLength);
  // Should generate an error, since zero-len settings frames are unsupported.
  EXPECT_EQ(1, visitor.error_count_);
}

// Tests handling of SETTINGS frames larger than the frame buffer size.
TEST_P(SpdyFramerTest, ReadLargeSettingsFrame) {
  SpdyFramer framer(spdy_version_);
  SettingsMap settings;
  SpdySettingsFlags flags = SETTINGS_FLAG_PLEASE_PERSIST;
  settings[SETTINGS_UPLOAD_BANDWIDTH] =
      SettingsFlagsAndValue(flags, 0x00000002);
  settings[SETTINGS_DOWNLOAD_BANDWIDTH] =
      SettingsFlagsAndValue(flags, 0x00000003);
  settings[SETTINGS_ROUND_TRIP_TIME] = SettingsFlagsAndValue(flags, 0x00000004);
  scoped_ptr<SpdyFrame> control_frame(framer.CreateSettings(settings));
  EXPECT_LT(SpdyFramer::kControlFrameBufferSize,
            control_frame->size());
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;

  // Read all at once.
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(settings.size(), static_cast<unsigned>(visitor.setting_count_));

  // Read data in small chunks.
  size_t framed_data = 0;
  size_t unframed_data = control_frame->size();
  size_t kReadChunkSize = 5;  // Read five bytes at a time.
  while (unframed_data > 0) {
    size_t to_read = min(kReadChunkSize, unframed_data);
    visitor.SimulateInFramer(
        reinterpret_cast<unsigned char*>(control_frame->data() + framed_data),
        to_read);
    unframed_data -= to_read;
    framed_data += to_read;
  }
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(settings.size() * 2, static_cast<unsigned>(visitor.setting_count_));
}

// Tests handling of SETTINGS frame with duplicate entries.
TEST_P(SpdyFramerTest, ReadDuplicateSettings) {
  SpdyFramer framer(spdy_version_);

  const unsigned char kV2FrameData[] = {
    0x80, spdy_version_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x01, 0x00, 0x00, 0x00,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x01, 0x00, 0x00, 0x00,  // 2nd (duplicate) Setting
    0x00, 0x00, 0x00, 0x03,
    0x03, 0x00, 0x00, 0x00,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };

  const unsigned char kV3FrameData[] = {
    0x80, spdy_version_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x01,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x01,  // 2nd (duplicate) Setting
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x03,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2FrameData, sizeof(kV2FrameData));
  } else {
    visitor.SimulateInFramer(kV3FrameData, sizeof(kV3FrameData));
  }
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(1, visitor.setting_count_);
}

// Tests handling of SETTINGS frame with entries out of order.
TEST_P(SpdyFramerTest, ReadOutOfOrderSettings) {
  SpdyFramer framer(spdy_version_);

  const unsigned char kV2FrameData[] = {
    0x80, spdy_version_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x02, 0x00, 0x00, 0x00,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x01, 0x00, 0x00, 0x00,  // 2nd (out of order) Setting
    0x00, 0x00, 0x00, 0x03,
    0x03, 0x00, 0x00, 0x00,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };

  const unsigned char kV3FrameData[] = {
    0x80, spdy_version_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x02,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x01,  // 2nd (out of order) Setting
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x01, 0x03,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2FrameData, sizeof(kV2FrameData));
  } else {
    visitor.SimulateInFramer(kV3FrameData, sizeof(kV3FrameData));
  }
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(1, visitor.setting_count_);
}

TEST_P(SpdyFramerTest, ReadWindowUpdate) {
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateWindowUpdate(1, 2));
  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.last_window_update_stream_);
  EXPECT_EQ(2, visitor.last_window_update_delta_);
}

TEST_P(SpdyFramerTest, ReadCredentialFrame) {
  SpdyCredential credential;
  credential.slot = 3;
  credential.proof = "proof";
  credential.certs.push_back("a cert");
  credential.certs.push_back("another cert");
  credential.certs.push_back("final cert");
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateCredentialFrame(credential));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(control_frame->size() - framer.GetControlFrameHeaderSize(),
            visitor.credential_buffer_length_);
  EXPECT_EQ(credential.slot, visitor.credential_.slot);
  EXPECT_EQ(credential.proof, visitor.credential_.proof);
  EXPECT_EQ(credential.certs.size(), visitor.credential_.certs.size());
  for (size_t i = 0; i < credential.certs.size(); i++) {
    EXPECT_EQ(credential.certs[i], visitor.credential_.certs[i]);
  }
}

TEST_P(SpdyFramerTest, ReadCredentialFrameOneByteAtATime) {
  SpdyCredential credential;
  credential.slot = 3;
  credential.proof = "proof";
  credential.certs.push_back("a cert");
  credential.certs.push_back("another cert");
  credential.certs.push_back("final cert");
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateCredentialFrame(credential));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  // Read one byte at a time to make sure we handle edge cases
  unsigned char* data =
      reinterpret_cast<unsigned char*>(control_frame->data());
  for (size_t idx = 0; idx < control_frame->size(); ++idx) {
    visitor.SimulateInFramer(data + idx, 1);
    ASSERT_EQ(0, visitor.error_count_);
  }
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(control_frame->size() - framer.GetControlFrameHeaderSize(),
            visitor.credential_buffer_length_);
  EXPECT_EQ(credential.slot, visitor.credential_.slot);
  EXPECT_EQ(credential.proof, visitor.credential_.proof);
  EXPECT_EQ(credential.certs.size(), visitor.credential_.certs.size());
  for (size_t i = 0; i < credential.certs.size(); i++) {
    EXPECT_EQ(credential.certs[i], visitor.credential_.certs[i]);
  }
}

TEST_P(SpdyFramerTest, ReadCredentialFrameWithNoPayload) {
  SpdyCredential credential;
  credential.slot = 3;
  credential.proof = "proof";
  credential.certs.push_back("a cert");
  credential.certs.push_back("another cert");
  credential.certs.push_back("final cert");
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateCredentialFrame(credential));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  SetFrameLength(control_frame.get(), 0, spdy_version_);
  unsigned char* data =
      reinterpret_cast<unsigned char*>(control_frame->data());
  visitor.SimulateInFramer(data, framer.GetControlFrameHeaderSize());
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadCredentialFrameWithCorruptProof) {
  SpdyCredential credential;
  credential.slot = 3;
  credential.proof = "proof";
  credential.certs.push_back("a cert");
  credential.certs.push_back("another cert");
  credential.certs.push_back("final cert");
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateCredentialFrame(credential));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  unsigned char* data =
      reinterpret_cast<unsigned char*>(control_frame->data());
  size_t offset = framer.GetControlFrameHeaderSize() + 4;
  data[offset] = 0xFF;  // Proof length is past the end of the frame
  visitor.SimulateInFramer(
      data, control_frame->size());
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadCredentialFrameWithCorruptCertificate) {
  SpdyCredential credential;
  credential.slot = 3;
  credential.proof = "proof";
  credential.certs.push_back("a cert");
  credential.certs.push_back("another cert");
  credential.certs.push_back("final cert");
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateCredentialFrame(credential));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  unsigned char* data =
      reinterpret_cast<unsigned char*>(control_frame->data());
  size_t offset = framer.GetCredentialMinimumSize() + 1;
  data[offset] = 0xFF;  // Proof length is past the end of the frame
  visitor.SimulateInFramer(
      data, control_frame->size());
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadGarbage) {
  SpdyFramer framer(spdy_version_);
  unsigned char garbage_frame[256];
  memset(garbage_frame, ~0, sizeof(garbage_frame));
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(garbage_frame, sizeof(garbage_frame));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadGarbageWithValidVersion) {
  SpdyFramer framer(spdy_version_);
  const unsigned char kFrameData[] = {
    0x80, spdy_version_, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
  };
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(kFrameData, arraysize(kFrameData));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, SizesTest) {
  SpdyFramer framer(spdy_version_);
  EXPECT_EQ(8u, framer.GetControlFrameHeaderSize());
  EXPECT_EQ(18u, framer.GetSynStreamMinimumSize());
  EXPECT_EQ(IsSpdy2() ? 14u : 12u, framer.GetSynReplyMinimumSize());
  EXPECT_EQ(16u, framer.GetRstStreamSize());
  EXPECT_EQ(8u, framer.GetControlFrameHeaderSize());
  EXPECT_EQ(18u, framer.GetSynStreamMinimumSize());
  EXPECT_EQ(IsSpdy2() ? 14u : 12u, framer.GetSynReplyMinimumSize());
  EXPECT_EQ(16u, framer.GetRstStreamSize());
  EXPECT_EQ(12u, framer.GetSettingsMinimumSize());
  EXPECT_EQ(12u, framer.GetPingSize());
  EXPECT_EQ(IsSpdy2() ? 12u : 16u, framer.GetGoAwaySize());
  EXPECT_EQ(IsSpdy2() ? 14u : 12u, framer.GetHeadersMinimumSize());
  EXPECT_EQ(16u, framer.GetWindowUpdateSize());
  EXPECT_EQ(10u, framer.GetCredentialMinimumSize());
}

TEST_P(SpdyFramerTest, StateToStringTest) {
  EXPECT_STREQ("ERROR",
               SpdyFramer::StateToString(SpdyFramer::SPDY_ERROR));
  EXPECT_STREQ("AUTO_RESET",
               SpdyFramer::StateToString(SpdyFramer::SPDY_AUTO_RESET));
  EXPECT_STREQ("RESET",
               SpdyFramer::StateToString(SpdyFramer::SPDY_RESET));
  EXPECT_STREQ("READING_COMMON_HEADER",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_READING_COMMON_HEADER));
  EXPECT_STREQ("CONTROL_FRAME_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_PAYLOAD));
  EXPECT_STREQ("IGNORE_REMAINING_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_IGNORE_REMAINING_PAYLOAD));
  EXPECT_STREQ("FORWARD_STREAM_FRAME",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_FORWARD_STREAM_FRAME));
  EXPECT_STREQ("SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK));
  EXPECT_STREQ("SPDY_CONTROL_FRAME_HEADER_BLOCK",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_HEADER_BLOCK));
  EXPECT_STREQ("SPDY_CREDENTIAL_FRAME_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CREDENTIAL_FRAME_PAYLOAD));
  EXPECT_STREQ("SPDY_SETTINGS_FRAME_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_SETTINGS_FRAME_PAYLOAD));
  EXPECT_STREQ("UNKNOWN_STATE",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_SETTINGS_FRAME_PAYLOAD + 1));
}

TEST_P(SpdyFramerTest, ErrorCodeToStringTest) {
  EXPECT_STREQ("NO_ERROR",
               SpdyFramer::ErrorCodeToString(SpdyFramer::SPDY_NO_ERROR));
  EXPECT_STREQ("INVALID_CONTROL_FRAME",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_INVALID_CONTROL_FRAME));
  EXPECT_STREQ("CONTROL_PAYLOAD_TOO_LARGE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_CONTROL_PAYLOAD_TOO_LARGE));
  EXPECT_STREQ("ZLIB_INIT_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_ZLIB_INIT_FAILURE));
  EXPECT_STREQ("UNSUPPORTED_VERSION",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_UNSUPPORTED_VERSION));
  EXPECT_STREQ("DECOMPRESS_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_DECOMPRESS_FAILURE));
  EXPECT_STREQ("COMPRESS_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_COMPRESS_FAILURE));
  EXPECT_STREQ("SPDY_INVALID_DATA_FRAME_FLAGS",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS));
  EXPECT_STREQ("SPDY_INVALID_CONTROL_FRAME_FLAGS",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS));
  EXPECT_STREQ("UNKNOWN_ERROR",
               SpdyFramer::ErrorCodeToString(SpdyFramer::LAST_ERROR));
}

TEST_P(SpdyFramerTest, StatusCodeToStringTest) {
  EXPECT_STREQ("INVALID",
               SpdyFramer::StatusCodeToString(RST_STREAM_INVALID));
  EXPECT_STREQ("PROTOCOL_ERROR",
               SpdyFramer::StatusCodeToString(RST_STREAM_PROTOCOL_ERROR));
  EXPECT_STREQ("INVALID_STREAM",
               SpdyFramer::StatusCodeToString(RST_STREAM_INVALID_STREAM));
  EXPECT_STREQ("REFUSED_STREAM",
               SpdyFramer::StatusCodeToString(RST_STREAM_REFUSED_STREAM));
  EXPECT_STREQ("UNSUPPORTED_VERSION",
               SpdyFramer::StatusCodeToString(RST_STREAM_UNSUPPORTED_VERSION));
  EXPECT_STREQ("CANCEL",
               SpdyFramer::StatusCodeToString(RST_STREAM_CANCEL));
  EXPECT_STREQ("INTERNAL_ERROR",
               SpdyFramer::StatusCodeToString(RST_STREAM_INTERNAL_ERROR));
  EXPECT_STREQ("FLOW_CONTROL_ERROR",
               SpdyFramer::StatusCodeToString(RST_STREAM_FLOW_CONTROL_ERROR));
  EXPECT_STREQ("UNKNOWN_STATUS",
               SpdyFramer::StatusCodeToString(RST_STREAM_NUM_STATUS_CODES));
}

TEST_P(SpdyFramerTest, ControlTypeToStringTest) {
  EXPECT_STREQ("SYN_STREAM",
               SpdyFramer::ControlTypeToString(SYN_STREAM));
  EXPECT_STREQ("SYN_REPLY",
               SpdyFramer::ControlTypeToString(SYN_REPLY));
  EXPECT_STREQ("RST_STREAM",
               SpdyFramer::ControlTypeToString(RST_STREAM));
  EXPECT_STREQ("SETTINGS",
               SpdyFramer::ControlTypeToString(SETTINGS));
  EXPECT_STREQ("NOOP",
               SpdyFramer::ControlTypeToString(NOOP));
  EXPECT_STREQ("PING",
               SpdyFramer::ControlTypeToString(PING));
  EXPECT_STREQ("GOAWAY",
               SpdyFramer::ControlTypeToString(GOAWAY));
  EXPECT_STREQ("HEADERS",
               SpdyFramer::ControlTypeToString(HEADERS));
  EXPECT_STREQ("WINDOW_UPDATE",
               SpdyFramer::ControlTypeToString(WINDOW_UPDATE));
  EXPECT_STREQ("CREDENTIAL",
               SpdyFramer::ControlTypeToString(CREDENTIAL));
  EXPECT_STREQ("UNKNOWN_CONTROL_TYPE",
               SpdyFramer::ControlTypeToString(NUM_CONTROL_FRAME_TYPES));
}

TEST_P(SpdyFramerTest, CatchProbableHttpResponse) {
  {
    testing::StrictMock<test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    EXPECT_CALL(visitor, OnError(_));
    framer.ProcessInput("HTTP/1.1", 8);
    EXPECT_TRUE(framer.probable_http_response());
    EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS, framer.error_code());
  }
  {
    testing::StrictMock<test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    EXPECT_CALL(visitor, OnError(_));
    framer.ProcessInput("HTTP/1.0", 8);
    EXPECT_TRUE(framer.probable_http_response());
    EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS, framer.error_code());
  }
}

TEST_P(SpdyFramerTest, DataFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    scoped_ptr<SpdyFrame> frame(
        framer.CreateDataFrame(1, "hello", 5, DATA_FLAG_NONE));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~DATA_FLAG_FIN) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnDataFrameHeader(1, 5, flags & DATA_FLAG_FIN));
      EXPECT_CALL(visitor, OnStreamFrameData(_, _, 5, SpdyDataFlags()));
      if (flags & DATA_FLAG_FIN) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, DATA_FLAG_FIN));
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~DATA_FLAG_FIN) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS,
                framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, SynStreamFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<net::test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    EXPECT_CALL(visitor, OnSynStreamCompressed(_, _));

    SpdyHeaderBlock headers;
    headers["foo"] = "bar";
    scoped_ptr<SpdyFrame> frame(
        framer.CreateSynStream(8, 3, 1, 0, CONTROL_FLAG_NONE, true, &headers));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~(CONTROL_FLAG_FIN | CONTROL_FLAG_UNIDIRECTIONAL)) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnSynStream(8, 3, 1, 0, flags & CONTROL_FLAG_FIN,
                                       flags & CONTROL_FLAG_UNIDIRECTIONAL));
      EXPECT_CALL(visitor, OnControlFrameHeaderData(8, _, _))
          .WillRepeatedly(testing::Return(true));
      if (flags & DATA_FLAG_FIN) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, DATA_FLAG_FIN));
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~(CONTROL_FLAG_FIN | CONTROL_FLAG_UNIDIRECTIONAL)) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, SynReplyFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<net::test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdyHeaderBlock headers;
    headers["foo"] = "bar";
    scoped_ptr<SpdyFrame> frame(
        framer.CreateSynReply(37, CONTROL_FLAG_NONE, true, &headers));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~CONTROL_FLAG_FIN) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnSynReply(37, flags & CONTROL_FLAG_FIN));
      EXPECT_CALL(visitor, OnControlFrameHeaderData(37, _, _))
          .WillRepeatedly(testing::Return(true));
      if (flags & DATA_FLAG_FIN) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, DATA_FLAG_FIN));
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~CONTROL_FLAG_FIN) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, RstStreamFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<net::test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    scoped_ptr<SpdyFrame> frame(framer.CreateRstStream(13, RST_STREAM_CANCEL));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnRstStream(13, RST_STREAM_CANCEL));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, SettingsFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<net::test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SettingsMap settings;
    settings[SETTINGS_UPLOAD_BANDWIDTH] =
        std::make_pair(SETTINGS_FLAG_NONE, 54321);
    scoped_ptr<SpdyFrame> frame(framer.CreateSettings(settings));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnSetting(SETTINGS_UPLOAD_BANDWIDTH,
                                     SETTINGS_FLAG_NONE, 54321));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, GoawayFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<net::test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    scoped_ptr<SpdyFrame> frame(framer.CreateGoAway(97, GOAWAY_OK));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnGoAway(97, GOAWAY_OK));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, HeadersFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<net::test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdyHeaderBlock headers;
    headers["foo"] = "bar";
    scoped_ptr<SpdyFrame> frame(
        framer.CreateHeaders(57, CONTROL_FLAG_NONE, true, &headers));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~CONTROL_FLAG_FIN) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnHeaders(57, flags & CONTROL_FLAG_FIN));
      EXPECT_CALL(visitor, OnControlFrameHeaderData(57, _, _))
          .WillRepeatedly(testing::Return(true));
      if (flags & DATA_FLAG_FIN) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, DATA_FLAG_FIN));
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~CONTROL_FLAG_FIN) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, PingFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<net::test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    scoped_ptr<SpdyFrame> frame(framer.CreatePingFrame(42));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnPing(42));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, WindowUpdateFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<net::test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    scoped_ptr<SpdyFrame> frame(framer.CreateWindowUpdate(4, 1024));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnWindowUpdate(4, 1024));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, CredentialFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<net::test::MockVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdyCredential credential;
    scoped_ptr<SpdyFrame> frame(framer.CreateCredentialFrame(credential));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnCredentialFrameData(_, _))
          .WillRepeatedly(testing::Return(true));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, EmptySynStream) {
  SpdyHeaderBlock headers;

  testing::StrictMock<test::MockVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnSynStreamCompressed(_, _));
  scoped_ptr<SpdyFrame>
      frame(framer.CreateSynStream(1, 0, 1, 0, CONTROL_FLAG_NONE, true,
                                   &headers));
  // Adjust size to remove the name/value block.
  SetFrameLength(
      frame.get(),
      framer.GetSynStreamMinimumSize() - framer.GetControlFrameHeaderSize(),
      spdy_version_);

  EXPECT_CALL(visitor, OnSynStream(1, 0, 1, 0, false, false));
  EXPECT_CALL(visitor, OnControlFrameHeaderData(1, NULL, 0));

  framer.ProcessInput(frame->data(), framer.GetSynStreamMinimumSize());
  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
}

TEST_P(SpdyFramerTest, SettingsFlagsAndId) {
  const uint32 kId = 0x020304;
  const uint32 kFlags = 0x01;
  const uint32 kWireFormat = htonl(IsSpdy2() ? 0x04030201 : 0x01020304);

  SettingsFlagsAndId id_and_flags =
      SettingsFlagsAndId::FromWireFormat(spdy_version_, kWireFormat);
  EXPECT_EQ(kId, id_and_flags.id());
  EXPECT_EQ(kFlags, id_and_flags.flags());
  EXPECT_EQ(kWireFormat, id_and_flags.GetWireFormat(spdy_version_));
}

// Test handling of a RST_STREAM with out-of-bounds status codes.
TEST_P(SpdyFramerTest, RstStreamStatusBounds) {
  DCHECK_GE(0xff, RST_STREAM_NUM_STATUS_CODES);

  const unsigned char kRstStreamInvalid[] = {
    0x80, spdy_version_, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, RST_STREAM_INVALID
  };

  const unsigned char kRstStreamNumStatusCodes[] = {
    0x80, spdy_version_, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, RST_STREAM_NUM_STATUS_CODES
  };

  testing::StrictMock<net::test::MockVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnRstStream(1, RST_STREAM_INVALID));
  framer.ProcessInput(reinterpret_cast<const char*>(kRstStreamInvalid),
                      arraysize(kRstStreamInvalid));
  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());

  EXPECT_CALL(visitor, OnRstStream(1, RST_STREAM_INVALID));
  framer.ProcessInput(reinterpret_cast<const char*>(kRstStreamNumStatusCodes),
                      arraysize(kRstStreamNumStatusCodes));
  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
}

// Tests handling of a GOAWAY frame with out-of-bounds stream ID.
TEST_P(SpdyFramerTest, GoAwayStreamIdBounds) {
  const unsigned char kV2FrameData[] = {
    0x80, spdy_version_, 0x00, 0x07,
    0x00, 0x00, 0x00, 0x04,
    0xff, 0xff, 0xff, 0xff,
  };
  const unsigned char kV3FrameData[] = {
    0x80, spdy_version_, 0x00, 0x07,
    0x00, 0x00, 0x00, 0x08,
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00,
  };

  testing::StrictMock<net::test::MockVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnGoAway(0x7fffffff, GOAWAY_OK));
  if (IsSpdy2()) {
    framer.ProcessInput(reinterpret_cast<const char*>(kV2FrameData),
                        arraysize(kV2FrameData));
  } else {
    framer.ProcessInput(reinterpret_cast<const char*>(kV3FrameData),
                        arraysize(kV3FrameData));
  }
  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code());
}

}  // namespace net
