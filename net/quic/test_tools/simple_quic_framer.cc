// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/simple_quic_framer.h"

#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"

using base::StringPiece;
using std::string;
using std::vector;

namespace net {
namespace test {

class SimpleFramerVisitor : public QuicFramerVisitorInterface {
 public:
  SimpleFramerVisitor()
      : error_(QUIC_NO_ERROR) {
  }

  virtual void OnError(QuicFramer* framer) {
    error_ = framer->error();
  }

  virtual void OnPacket() {}
  virtual void OnPublicResetPacket(const QuicPublicResetPacket& packet) {}
  virtual void OnRevivedPacket() {}
  virtual bool OnPacketHeader(const QuicPacketHeader& header) {
    has_header_ = true;
    header_ = header;
    return true;
  }

  virtual void OnFecProtectedPayload(StringPiece payload) {}

  virtual void OnStreamFrame(const QuicStreamFrame& frame) {
    // Save a copy of the data so it is valid after the packet is processed.
    stream_data_.push_back(frame.data.as_string());
    QuicStreamFrame stream_frame(frame);
    // Make sure that the stream frame points to this data.
    stream_frame.data = stream_data_.back();
    stream_frames_.push_back(stream_frame);
  }

  virtual void OnAckFrame(const QuicAckFrame& frame) {
    ack_frames_.push_back(frame);
  }

  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) {
    feedback_frames_.push_back(frame);
  }

  virtual void OnFecData(const QuicFecData& fec) {
    fec_data_ = fec;
    fec_redundancy_ = fec_data_.redundancy.as_string();
    fec_data_.redundancy = fec_redundancy_;
  }

  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) {
    rst_stream_frames_.push_back(frame);
  }

  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) {
    connection_close_frames_.push_back(frame);
  }

  virtual void OnGoAwayFrame(const QuicGoAwayFrame& frame) {
    goaway_frames_.push_back(frame);
  }

  virtual void OnPacketComplete() {}

  const QuicPacketHeader& header() const { return header_; }
  const vector<QuicAckFrame>& ack_frames() const { return ack_frames_; }
  const vector<QuicConnectionCloseFrame>& connection_close_frames() const {
    return connection_close_frames_;
  }
  const vector<QuicCongestionFeedbackFrame>& feedback_frames() const {
    return feedback_frames_;
  }
  const vector<QuicGoAwayFrame>& goaway_frames() const {
    return goaway_frames_;
  }
  const vector<QuicRstStreamFrame>& rst_stream_frames() const {
    return rst_stream_frames_;
  }
  const vector<QuicStreamFrame>& stream_frames() const {
    return stream_frames_;
  }
  const QuicFecData& fec_data() const {
    return fec_data_;
  }

 private:
  QuicErrorCode error_;
  bool has_header_;
  QuicPacketHeader header_;
  QuicFecData fec_data_;
  string fec_redundancy_;
  vector<QuicAckFrame> ack_frames_;
  vector<QuicCongestionFeedbackFrame> feedback_frames_;
  vector<QuicStreamFrame> stream_frames_;
  vector<QuicRstStreamFrame> rst_stream_frames_;
  vector<QuicGoAwayFrame> goaway_frames_;
  vector<QuicConnectionCloseFrame> connection_close_frames_;
  vector<string> stream_data_;

  DISALLOW_COPY_AND_ASSIGN(SimpleFramerVisitor);
};

SimpleQuicFramer::SimpleQuicFramer()
    : framer_(kQuicVersion1,
              QuicDecrypter::Create(kNULL),
              QuicEncrypter::Create(kNULL)) {
}

SimpleQuicFramer::~SimpleQuicFramer() {
}

bool SimpleQuicFramer::ProcessPacket(const QuicPacket& packet) {
  scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(0, packet));
  return ProcessPacket(*encrypted);
}

bool SimpleQuicFramer::ProcessPacket(const QuicEncryptedPacket& packet) {
  visitor_.reset(new SimpleFramerVisitor);
  framer_.set_visitor(visitor_.get());
  return framer_.ProcessPacket(packet);
}

const QuicPacketHeader& SimpleQuicFramer::header() const {
  return visitor_->header();
}

const QuicFecData& SimpleQuicFramer::fec_data() const {
  return visitor_->fec_data();
}

size_t SimpleQuicFramer::num_frames() const {
  return ack_frames().size() +
      stream_frames().size() +
      feedback_frames().size() +
      rst_stream_frames().size() +
      goaway_frames().size() +
      connection_close_frames().size();
}

const vector<QuicAckFrame>& SimpleQuicFramer::ack_frames() const {
  return visitor_->ack_frames();
}

const vector<QuicStreamFrame>& SimpleQuicFramer::stream_frames() const {
  return visitor_->stream_frames();
}

const vector<QuicRstStreamFrame>& SimpleQuicFramer::rst_stream_frames() const {
  return visitor_->rst_stream_frames();
}

const vector<QuicCongestionFeedbackFrame>&
SimpleQuicFramer::feedback_frames() const {
  return visitor_->feedback_frames();
}

const vector<QuicGoAwayFrame>&
SimpleQuicFramer::goaway_frames() const {
  return visitor_->goaway_frames();
}

const vector<QuicConnectionCloseFrame>&
SimpleQuicFramer::connection_close_frames() const {
  return visitor_->connection_close_frames();
}

}  // namespace test
}  // namespace net
