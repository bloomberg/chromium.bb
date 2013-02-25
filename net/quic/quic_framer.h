// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_FRAMER_H_
#define NET_QUIC_QUIC_FRAMER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"

namespace net {

namespace test {
class QuicFramerPeer;
}  // namespace test

class QuicDataReader;
class QuicDataWriter;
class QuicDecrypter;
class QuicEncrypter;
class QuicFramer;

// This class receives callbacks from the framer when packets
// are processed.
class NET_EXPORT_PRIVATE QuicFramerVisitorInterface {
 public:
  virtual ~QuicFramerVisitorInterface() {}

  // Called if an error is detected in the QUIC protocol.
  virtual void OnError(QuicFramer* framer) = 0;

  // Called when a new packet has been received, before it
  // has been validated or processed.
  virtual void OnPacket() = 0;

  // Called when a public reset packet has been parsed but has not yet
  // been validated.
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) = 0;

  // Called when a lost packet has been recovered via FEC,
  // before it has been processed.
  virtual void OnRevivedPacket() = 0;

  // Called when the header of a packet had been parsed.
  // If OnPacketHeader returns false, framing for this packet will cease.
  virtual bool OnPacketHeader(const QuicPacketHeader& header) = 0;

  // Called when a data packet is parsed that is part of an FEC group.
  // |payload| is the non-encrypted FEC protected payload of the packet.
  virtual void OnFecProtectedPayload(base::StringPiece payload) = 0;

  // Called when a StreamFrame has been parsed.
  virtual void OnStreamFrame(const QuicStreamFrame& frame) = 0;

  // Called when a AckFrame has been parsed.
  virtual void OnAckFrame(const QuicAckFrame& frame) = 0;

  // Called when a CongestionFeedbackFrame has been parsed.
  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) = 0;

  // Called when a RstStreamFrame has been parsed.
  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) = 0;

  // Called when a ConnectionCloseFrame has been parsed.
  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) = 0;

  // Called when a GoAwayFrame has been parsed.
  virtual void OnGoAwayFrame(const QuicGoAwayFrame& frame) = 0;

  // Called when FEC data has been parsed.
  virtual void OnFecData(const QuicFecData& fec) = 0;

  // Called when a packet has been completely processed.
  virtual void OnPacketComplete() = 0;
};

class NET_EXPORT_PRIVATE QuicFecBuilderInterface {
 public:
  virtual ~QuicFecBuilderInterface() {}

  // Called when a data packet is constructed that is part of an FEC group.
  // |payload| is the non-encrypted FEC protected payload of the packet.
  virtual void OnBuiltFecProtectedPayload(const QuicPacketHeader& header,
                                          base::StringPiece payload) = 0;
};

// This class calculates the received entropy of the ack packet being
// framed, should it get truncated.
class NET_EXPORT_PRIVATE QuicReceivedEntropyHashCalculatorInterface {
 public:
  virtual ~QuicReceivedEntropyHashCalculatorInterface() {}

  // When an ack frame gets truncated while being framed the received
  // entropy of the ack frame needs to be calculated since the some of the
  // missing packets are not added and the largest observed might be lowered.
  // This should return the received entropy hash of the packets received up to
  // and including |sequence_number|.
  virtual QuicPacketEntropyHash ReceivedEntropyHash(
      QuicPacketSequenceNumber sequence_number) const = 0;
};

// Class for parsing and constructing QUIC packets.  It has a
// QuicFramerVisitorInterface that is called when packets are parsed.
// It also has a QuicFecBuilder that is called when packets are constructed
// in order to generate FEC data for subsequently building FEC packets.
class NET_EXPORT_PRIVATE QuicFramer {
 public:
  // Constructs a new framer that will own |decrypter| and |encrypter|.
  QuicFramer(QuicDecrypter* decrypter, QuicEncrypter* encrypter);

  virtual ~QuicFramer();

  // Calculates the largest observed packet to advertise in the case an Ack
  // Frame was truncated.  last_written in this case is the iterator for the
  // last missing packet which fit in the outgoing ack.
  static QuicPacketSequenceNumber CalculateLargestObserved(
      const SequenceNumberSet& missing_packets,
      SequenceNumberSet::const_iterator last_written);

  // Set callbacks to be called from the framer.  A visitor must be set, or
  // else the framer will likely crash.  It is acceptable for the visitor
  // to do nothing.  If this is called multiple times, only the last visitor
  // will be used.
  void set_visitor(QuicFramerVisitorInterface* visitor) {
    visitor_ = visitor;
  }

  // Set a builder to be called from the framer when building FEC protected
  // packets.  If this is called multiple times, only the last builder
  // will be used.  The builder need not be set.
  void set_fec_builder(QuicFecBuilderInterface* builder) {
    fec_builder_ = builder;
  }

  // Set entropy calculator to be called from the framer when it needs the
  // entropy of a truncated ack frame. An entropy calculator must be set or else
  // the framer will likely crash. If this is called multiple times, only the
  // last visitor will be used.
  void set_entropy_calculator(
      QuicReceivedEntropyHashCalculatorInterface* entropy_calculator) {
    entropy_calculator_ = entropy_calculator;
  }

  QuicErrorCode error() const {
    return error_;
  }

  // Pass a UDP packet into the framer for parsing.
  // Return true if the packet was processed succesfully. |packet| must be a
  // single, complete UDP packet (not a frame of a packet).  This packet
  // might be null padded past the end of the payload, which will be correctly
  // ignored.
  bool ProcessPacket(const QuicEncryptedPacket& packet);

  // Pass a data packet that was revived from FEC data into the framer
  // for parsing.
  // Return true if the packet was processed succesfully. |payload| must be
  // the complete DECRYPTED payload of the revived packet.
  bool ProcessRevivedPacket(QuicPacketHeader* header,
                            base::StringPiece payload);

  // Returns the number of bytes added to the packet for the specified frame,
  // and 0 if the frame doesn't fit.  Includes the header size for the first
  // frame.
  size_t GetSerializedFrameLength(
      const QuicFrame& frame, size_t free_bytes, bool first_frame);

  // Returns a SerializedPacket whose |packet| member is owned by the caller,
  // and is populated with the fields in |header| and |frames|, or is NULL if
  // the packet could not be created.
  // TODO(ianswett): Used for testing only.
  SerializedPacket ConstructFrameDataPacket(const QuicPacketHeader& header,
                                            const QuicFrames& frames);

  // Returns a SerializedPacket whose |packet| member is owned by the caller,
  // is created from the first |num_frames| frames, or is NULL if the packet
  // could not be created.  The packet must be of size |packet_size|.
  SerializedPacket ConstructFrameDataPacket(const QuicPacketHeader& header,
                                            const QuicFrames& frames,
                                            size_t packet_size);

  // Returns a SerializedPacket whose |packet| member is owned by the caller,
  // and is populated with the fields in |header| and |fec|, or is NULL if the
  // packet could not be created.
  SerializedPacket ConstructFecPacket(const QuicPacketHeader& header,
                                 const QuicFecData& fec);

  // Returns a new public reset packet, owned by the caller.
  static QuicEncryptedPacket* ConstructPublicResetPacket(
      const QuicPublicResetPacket& packet);

  // Returns a new encrypted packet, owned by the caller.
  QuicEncryptedPacket* EncryptPacket(QuicPacketSequenceNumber sequence_number,
                                     const QuicPacket& packet);

  // Returns the maximum length of plaintext that can be encrypted
  // to ciphertext no larger than |ciphertext_size|.
  size_t GetMaxPlaintextSize(size_t ciphertext_size);

  const std::string& detailed_error() { return detailed_error_; }

  // Read the guid from a packet header.
  // Return true on success, else false.
  static bool ReadGuidFromPacket(const QuicEncryptedPacket& packet,
                                 QuicGuid* guid);

 private:
  friend class test::QuicFramerPeer;

  QuicPacketEntropyHash GetPacketEntropyHash(
      const QuicPacketHeader& header) const;

  bool ProcessDataPacket(const QuicPacketPublicHeader& public_header,
                         const QuicEncryptedPacket& packet);

  bool ProcessPublicResetPacket(const QuicPacketPublicHeader& public_header);

  bool WritePacketHeader(const QuicPacketHeader& header,
                         QuicDataWriter* writer);

  bool ProcessPublicHeader(QuicPacketPublicHeader* header);

  bool ProcessPacketHeader(QuicPacketHeader* header,
                           const QuicEncryptedPacket& packet);

  bool ProcessPacketSequenceNumber(QuicPacketSequenceNumber* sequence_number);
  bool ProcessFrameData();
  bool ProcessStreamFrame();
  bool ProcessAckFrame(QuicAckFrame* frame);
  bool ProcessReceivedInfo(ReceivedPacketInfo* received_info);
  bool ProcessSentInfo(SentPacketInfo* sent_info);
  bool ProcessQuicCongestionFeedbackFrame(
      QuicCongestionFeedbackFrame* congestion_feedback);
  bool ProcessRstStreamFrame();
  bool ProcessConnectionCloseFrame();
  bool ProcessGoAwayFrame();

  bool DecryptPayload(QuicPacketSequenceNumber packet_sequence_number,
                      const QuicEncryptedPacket& packet);

  // Returns the full packet sequence number from the truncated
  // wire format version and the last seen packet sequence number.
  QuicPacketSequenceNumber CalculatePacketSequenceNumberFromWire(
      QuicPacketSequenceNumber packet_sequence_number) const;

  // Computes the wire size in bytes of the payload of |frame|.
  size_t ComputeFramePayloadLength(const QuicFrame& frame);

  static bool AppendPacketSequenceNumber(
      QuicPacketSequenceNumber packet_sequence_number,
      QuicDataWriter* writer);

  bool AppendStreamFramePayload(const QuicStreamFrame& frame,
                                QuicDataWriter* builder);
  bool AppendAckFramePayload(const QuicAckFrame& frame,
                             QuicDataWriter* builder);
  bool AppendQuicCongestionFeedbackFramePayload(
      const QuicCongestionFeedbackFrame& frame,
      QuicDataWriter* builder);
  bool AppendRstStreamFramePayload(const QuicRstStreamFrame& frame,
                                   QuicDataWriter* builder);
  bool AppendConnectionCloseFramePayload(
      const QuicConnectionCloseFrame& frame,
      QuicDataWriter* builder);
  bool AppendGoAwayFramePayload(const QuicGoAwayFrame& frame,
                                QuicDataWriter* writer);
  bool RaiseError(QuicErrorCode error);

  void set_error(QuicErrorCode error) {
    error_ = error;
  }

  void set_detailed_error(const char* error) {
    detailed_error_ = error;
  }

  std::string detailed_error_;
  scoped_ptr<QuicDataReader> reader_;
  QuicFramerVisitorInterface* visitor_;
  QuicFecBuilderInterface* fec_builder_;
  QuicReceivedEntropyHashCalculatorInterface* entropy_calculator_;
  QuicErrorCode error_;
  QuicPacketSequenceNumber last_sequence_number_;
  // Buffer containing decrypted payload data during parsing.
  scoped_ptr<QuicData> decrypted_;
  // Decrypter used to decrypt packets during parsing.
  scoped_ptr<QuicDecrypter> decrypter_;
  // Encrypter used to encrypt packets via EncryptPacket().
  scoped_ptr<QuicEncrypter> encrypter_;

  DISALLOW_COPY_AND_ASSIGN(QuicFramer);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_FRAMER_H_
