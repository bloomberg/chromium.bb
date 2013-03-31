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
#include "net/quic/quic_protocol.h"

namespace net {

namespace test {
class QuicFramerPeer;
}  // namespace test

class QuicDataReader;
class QuicDataWriter;
class QuicDecrypter;
class QuicEncrypter;
class QuicFramer;

// Number of bytes reserved for the frame type preceding each frame.
const size_t kQuicFrameTypeSize = 1;
// Number of bytes reserved for error code.
const size_t kQuicErrorCodeSize = 4;
// Number of bytes reserved to denote the length of error details field.
const size_t kQuicErrorDetailsLengthSize = 2;

// Number of bytes reserved for stream id
const size_t kQuicStreamIdSize = 4;
// Number of bytes reserved for fin flag in stream frame.
const size_t kQuicStreamFinSize = 1;
// Number of bytes reserved for byte offset in stream frame.
const size_t kQuicStreamOffsetSize = 8;
// Number of bytes reserved to store payload length in stream frame.
const size_t kQuicStreamPayloadLengthSize = 2;

// Size in bytes of the entropy hash sent in ack frames.
const size_t kQuicEntropyHashSize = 1;
// Size in bytes reserved for the number of missing packets in ack frames.
const size_t kNumberOfMissingPacketsSize = 1;

// This class receives callbacks from the framer when packets
// are processed.
class NET_EXPORT_PRIVATE QuicFramerVisitorInterface {
 public:
  virtual ~QuicFramerVisitorInterface() {}

  // Called if an error is detected in the QUIC protocol.
  virtual void OnError(QuicFramer* framer) = 0;

  // Called only when |is_server_| is true and the the framer gets a packet with
  // version flag true and the version on the packet doesn't match
  // |quic_version_|. The visitor should return true after it updates the
  // version of the |framer_| to |received_version| or false to stop processing
  // this packet.
  virtual bool OnProtocolVersionMismatch(QuicVersionTag received_version) = 0;

  // Called when a new packet has been received, before it
  // has been validated or processed.
  virtual void OnPacket() = 0;

  // Called when a public reset packet has been parsed but has not yet
  // been validated.
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) = 0;

  // Called only when |is_server_| is false and a version negotiation packet has
  // been parsed.
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) = 0;

  // Called when a lost packet has been recovered via FEC,
  // before it has been processed.
  virtual void OnRevivedPacket() = 0;

  // Called when the complete header of a packet had been parsed.
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
  QuicFramer(QuicVersionTag quic_version,
             QuicDecrypter* decrypter,
             QuicEncrypter* encrypter,
             QuicTime creation_time,
             bool is_server);

  virtual ~QuicFramer();

  // Returns true if |version| is a supported protocol version.
  bool IsSupportedVersion(QuicVersionTag version);

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

  QuicVersionTag version() const {
    return quic_version_;
  }

  void set_version(QuicVersionTag version) {
    DCHECK(IsSupportedVersion(version));
    quic_version_ = version;
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

  // Size in bytes of all stream frame fields without the payload.
  static size_t GetMinStreamFrameSize();
  // Size in bytes of all ack frame fields without the missing packets.
  static size_t GetMinAckFrameSize();
  // Size in bytes of all reset stream frame without the error details.
  static size_t GetMinRstStreamFrameSize();
  // Size in bytes of all connection close frame fields without the error
  // details and the missing packets from the enclosed ack frame.
  static size_t GetMinConnectionCloseFrameSize();
  // Size in bytes of all GoAway frame fields without the reason phrase.
  static size_t GetMinGoAwayFrameSize();
  // Size in bytes required for a serialized version negotiation packet
  size_t GetVersionNegotiationPacketSize(size_t number_versions);

  // Returns the number of bytes added to the packet for the specified frame,
  // and 0 if the frame doesn't fit.  Includes the header size for the first
  // frame.
  size_t GetSerializedFrameLength(
      const QuicFrame& frame, size_t free_bytes, bool first_frame);

  // Returns the associated data from the encrypted packet |encrypted| as a
  // stringpiece.
  static base::StringPiece GetAssociatedDataFromEncryptedPacket(
      const QuicEncryptedPacket& encrypted, bool includes_version);

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

  QuicEncryptedPacket* ConstructVersionNegotiationPacket(
      const QuicPacketPublicHeader& header,
      const QuicVersionTagList& supported_versions);

  QuicDecrypter* decrypter() const { return decrypter_.get(); }
  void push_decrypter(QuicDecrypter* decrypter);
  void pop_decrypter();

  QuicEncrypter* encrypter() const { return encrypter_.get(); }
  void set_encrypter(QuicEncrypter* encrypter);

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

  bool ProcessVersionNegotiationPacket(QuicPacketPublicHeader* public_header);

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
                      bool version_flag,
                      const QuicEncryptedPacket& packet);

  // Returns the full packet sequence number from the truncated
  // wire format version and the last seen packet sequence number.
  QuicPacketSequenceNumber CalculatePacketSequenceNumberFromWire(
      QuicPacketSequenceNumber packet_sequence_number) const;

  // Computes the wire size in bytes of the payload of |frame|.
  size_t ComputeFrameLength(const QuicFrame& frame);

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
  // Updated by ProcessPacketHeader when it succeeds.
  QuicPacketSequenceNumber last_sequence_number_;
  // Buffer containing decrypted payload data during parsing.
  scoped_ptr<QuicData> decrypted_;
  // Version of the protocol being used.
  QuicVersionTag quic_version_;
  // Primary decrypter used to decrypt packets during parsing.
  scoped_ptr<QuicDecrypter> decrypter_;
  // Backup decrypter used to decrypt packets during parsing. May be NULL.
  scoped_ptr<QuicDecrypter> backup_decrypter_;
  // Encrypter used to encrypt packets via EncryptPacket().
  scoped_ptr<QuicEncrypter> encrypter_;
  // Tracks if the framer is being used by the entity that received the
  // connection or the entity that initiated it.
  bool is_server_;
  // The time this frames was created.  Time written to the wire will be
  // written as a delta from this value.
  QuicTime creation_time_;

  DISALLOW_COPY_AND_ASSIGN(QuicFramer);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_FRAMER_H_
