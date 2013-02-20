// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_PROTOCOL_H_
#define NET_QUIC_QUIC_PROTOCOL_H_

#include <stddef.h>
#include <limits>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "net/base/int128.h"
#include "net/base/net_export.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_time.h"

namespace net {

using ::operator<<;

class QuicPacket;

typedef uint64 QuicGuid;
typedef uint32 QuicStreamId;
typedef uint64 QuicStreamOffset;
typedef uint64 QuicPacketSequenceNumber;
typedef QuicPacketSequenceNumber QuicFecGroupNumber;
typedef uint64 QuicPublicResetNonceProof;

// TODO(rch): Consider Quic specific names for these constants.
// Maximum size in bytes of a QUIC packet.
const QuicByteCount kMaxPacketSize = 1200;

// Maximum number of open streams per connection.
const size_t kDefaultMaxStreamsPerConnection = 100;

// Number of bytes reserved for guid in the packet header.
const size_t kQuicGuidSize = 8;
// Number of bytes reserved for public flags in the packet header.
const size_t kPublicFlagsSize = 1;
// Number of bytes reserved for sequence number in the packet header.
const size_t kSequenceNumberSize = 6;
// Number of bytes reserved for private flags in the packet header.
const size_t kPrivateFlagsSize = 1;
// Number of bytes reserved for FEC group in the packet header.
const size_t kFecGroupSize = 1;
// Number of bytes reserved for the nonce proof in public reset packet.
const size_t kPublicResetNonceSize = 8;
// Number of bytes reserved for the frame type preceding each frame.
const size_t kFrameTypeSize = 1;

// Size in bytes of the data or fec packet header.
const size_t kPacketHeaderSize = kQuicGuidSize + kPublicFlagsSize +
    kPrivateFlagsSize + kSequenceNumberSize + kFecGroupSize;
// Size in bytes of the public reset packet.
const size_t kPublicResetPacketSize = kQuicGuidSize + kPublicFlagsSize +
    kPublicResetNonceSize + kSequenceNumberSize;

// Index into the guid offset in the header.
const size_t kGuidOffset = 0;
// Index into the flags offset in the header.
const size_t kPublicFlagsOffset = kQuicGuidSize;

// Index into the sequence number offset in the header.
const size_t kSequenceNumberOffset = kPublicFlagsOffset + kPublicFlagsSize;
// Index into the private flags offset in the data packet header.
const size_t kPrivateFlagsOffset = kSequenceNumberOffset + kSequenceNumberSize;
// Index into the fec group offset in the header.
const size_t kFecGroupOffset = kPrivateFlagsOffset + kPrivateFlagsSize;

// Index into the nonce proof of the public reset packet.
const size_t kPublicResetPacketNonceProofOffset = kPublicFlagsOffset +
    kPublicFlagsSize;
// Index into the rejected sequence number of the public reset packet.
const size_t kPublicResetPacketRejectedSequenceNumberOffset =
    kPublicResetPacketNonceProofOffset + kPublicResetNonceSize;

// Index of the first byte in a QUIC packet of FEC protected data.
const size_t kStartOfFecProtectedData = kPacketHeaderSize;
// Index of the first byte in a QUIC packet of encrypted data.
const size_t kStartOfEncryptedData = kPacketHeaderSize - kPrivateFlagsSize -
    kFecGroupSize;
// Index of the first byte in a QUIC packet which is hashed.
const size_t kStartOfHashData = 0;

// Size in bytes of all stream frame fields.
const size_t kMinStreamFrameLength = 15;

// Limit on the delta between stream IDs.
const QuicStreamId kMaxStreamIdDelta = 100;

// Reserved ID for the crypto stream.
// TODO(rch): ensure that this is not usable by any other streams.
const QuicStreamId kCryptoStreamId = 1;

// Value which indicates this packet is not FEC protected.
const uint8 kNoFecOffset = 0xFF;

typedef std::pair<QuicPacketSequenceNumber, QuicPacket*> PacketPair;

const int64 kDefaultTimeoutUs = 600000000;  // 10 minutes.

enum QuicFrameType {
  PADDING_FRAME = 0,
  STREAM_FRAME,
  ACK_FRAME,
  CONGESTION_FEEDBACK_FRAME,
  RST_STREAM_FRAME,
  CONNECTION_CLOSE_FRAME,
  NUM_FRAME_TYPES
};

enum QuicPacketPublicFlags {
  PACKET_PUBLIC_FLAGS_NONE = 0,
  PACKET_PUBLIC_FLAGS_VERSION = 1,
  PACKET_PUBLIC_FLAGS_RST = 2,  // Packet is a public reset packet.
  PACKET_PUBLIC_FLAGS_MAX = 3  // Both bit set.
};

enum QuicPacketPrivateFlags {
  PACKET_PRIVATE_FLAGS_NONE = 0,
  PACKET_PRIVATE_FLAGS_FEC = 1,  // Payload is FEC as opposed to frames.
  PACKET_PRIVATE_FLAGS_MAX = PACKET_PRIVATE_FLAGS_FEC
};

enum QuicVersion {
  QUIC_VERSION_1 = 0
};

enum QuicErrorCode {
  // Stream errors.
  QUIC_NO_ERROR = 0,

  // There were data frames after the a fin or reset.
  QUIC_STREAM_DATA_AFTER_TERMINATION,
  // There was some server error which halted stream processing.
  QUIC_SERVER_ERROR_PROCESSING_STREAM,
  // We got two fin or reset offsets which did not match.
  QUIC_MULTIPLE_TERMINATION_OFFSETS,
  // We got bad payload and can not respond to it at the protocol level.
  QUIC_BAD_APPLICATION_PAYLOAD,

  // Connection errors.

  // Control frame is malformed.
  QUIC_INVALID_PACKET_HEADER,
  // Frame data is malformed.
  QUIC_INVALID_FRAME_DATA,
  // FEC data is malformed.
  QUIC_INVALID_FEC_DATA,
  // Stream rst data is malformed
  QUIC_INVALID_RST_STREAM_DATA,
  // Connection close data is malformed.
  QUIC_INVALID_CONNECTION_CLOSE_DATA,
  // Ack data is malformed.
  QUIC_INVALID_ACK_DATA,
  // There was an error decrypting.
  QUIC_DECRYPTION_FAILURE,
  // There was an error encrypting.
  QUIC_ENCRYPTION_FAILURE,
  // The packet exceeded kMaxPacketSize.
  QUIC_PACKET_TOO_LARGE,
  // Data was sent for a stream which did not exist.
  QUIC_PACKET_FOR_NONEXISTENT_STREAM,
  // The client is going away (browser close, etc.)
  QUIC_CLIENT_GOING_AWAY,
  // The server is going away (restart etc.)
  QUIC_SERVER_GOING_AWAY,
  // A stream ID was invalid.
  QUIC_INVALID_STREAM_ID,
  // Too many streams already open.
  QUIC_TOO_MANY_OPEN_STREAMS,
  // Received public reset for this connection.
  QUIC_PUBLIC_RESET,

  // We hit our prenegotiated (or default) timeout
  QUIC_CONNECTION_TIMED_OUT,

  // Crypto errors.

  // Handshake message contained out of order tags.
  QUIC_CRYPTO_TAGS_OUT_OF_ORDER,
  // Handshake message contained too many entries.
  QUIC_CRYPTO_TOO_MANY_ENTRIES,
  // Handshake message contained an invalid value length.
  QUIC_CRYPTO_INVALID_VALUE_LENGTH,
  // A crypto message was received after the handshake was complete.
  QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE,
  // A crypto message was received with an illegal message tag.
  QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
  // A crypto message was received with an illegal parameter.
  QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER,
  // A crypto message was received with a mandatory parameter missing.
  QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND,
  // A crypto message was received with a parameter that has no overlap
  // with the local parameter.
  QUIC_CRYPTO_MESSAGE_PARAMETER_NO_OVERLAP,
};

struct NET_EXPORT_PRIVATE QuicPacketPublicHeader {
  // Universal header. All QuicPacket headers will have a guid and public flags.
  // TODO(satyamshekhar): Support versioning as per Protocol Negotiation Doc.
  QuicGuid guid;
  QuicPacketPublicFlags flags;
};

// Header for Data or FEC packets.
struct QuicPacketHeader {
  QuicPacketHeader() {}
  explicit QuicPacketHeader(const QuicPacketPublicHeader& header)
      : public_header(header) {}
  QuicPacketPublicHeader public_header;
  QuicPacketPrivateFlags private_flags;
  QuicPacketSequenceNumber packet_sequence_number;
  QuicFecGroupNumber fec_group;
};

struct QuicPublicResetPacket {
  QuicPublicResetPacket() {}
  explicit QuicPublicResetPacket(const QuicPacketPublicHeader& header)
      : public_header(header) {}
  QuicPacketPublicHeader public_header;
  QuicPacketSequenceNumber rejected_sequence_number;
  QuicPublicResetNonceProof nonce_proof;
};

// A padding frame contains no payload.
struct NET_EXPORT_PRIVATE QuicPaddingFrame {
};

struct NET_EXPORT_PRIVATE QuicStreamFrame {
  QuicStreamFrame();
  QuicStreamFrame(QuicStreamId stream_id,
                  bool fin,
                  QuicStreamOffset offset,
                  base::StringPiece data);

  QuicStreamId stream_id;
  bool fin;
  QuicStreamOffset offset;  // Location of this data in the stream.
  base::StringPiece data;
};

// TODO(ianswett): Re-evaluate the trade-offs of hash_set vs set when framing
// is finalized.
typedef std::set<QuicPacketSequenceNumber> SequenceSet;
// TODO(pwestin): Add a way to enforce the max size of this map.
typedef std::map<QuicPacketSequenceNumber, QuicTime> TimeMap;

struct NET_EXPORT_PRIVATE ReceivedPacketInfo {
  ReceivedPacketInfo();
  ~ReceivedPacketInfo();
  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const ReceivedPacketInfo& s);

  // Records a packet receipt.
  void RecordReceived(QuicPacketSequenceNumber sequence_number);

  // True if the sequence number is greater than largest_observed or is listed
  // as missing.
  // Always returns false for sequence numbers less than least_unacked.
  bool IsAwaitingPacket(QuicPacketSequenceNumber sequence_number) const;

  // Clears all missing packets less than |least_unacked|.
  void ClearMissingBefore(QuicPacketSequenceNumber least_unacked);

  // The highest packet sequence number we've observed from the peer.
  //
  // In general, this should be the largest packet number we've received.  In
  // the case of truncated acks, we may have to advertise a lower "upper bound"
  // than largest received, to avoid implicitly acking missing packets that
  // don't fit in the missing packet list due to size limitations.  In this
  // case, largest_observed may be a packet which is also in the missing packets
  // list.
  QuicPacketSequenceNumber largest_observed;

  // The set of packets which we're expecting and have not received.
  SequenceSet missing_packets;
};

struct NET_EXPORT_PRIVATE SentPacketInfo {
  SentPacketInfo();
  ~SentPacketInfo();
  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const SentPacketInfo& s);

  // The lowest packet we've sent which is unacked, and we expect an ack for.
  QuicPacketSequenceNumber least_unacked;
};

struct NET_EXPORT_PRIVATE QuicAckFrame {
  QuicAckFrame() {}
  // Testing convenience method to construct a QuicAckFrame with all packets
  // from least_unacked to largest_observed acked.
  QuicAckFrame(QuicPacketSequenceNumber largest_observed,
               QuicPacketSequenceNumber least_unacked);

  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicAckFrame& s);

  SentPacketInfo sent_info;
  ReceivedPacketInfo received_info;
};

// Defines for all types of congestion feedback that will be negotiated in QUIC,
// kTCP MUST be supported by all QUIC implementations to guarentee 100%
// compatibility.
enum CongestionFeedbackType {
  kTCP,  // Used to mimic TCP.
  kInterArrival,  // Use additional inter arrival information.
  kFixRate,  // Provided for testing.
};

struct NET_EXPORT_PRIVATE CongestionFeedbackMessageTCP {
  uint16 accumulated_number_of_lost_packets;
  QuicByteCount receive_window;
};

struct NET_EXPORT_PRIVATE CongestionFeedbackMessageInterArrival {
  CongestionFeedbackMessageInterArrival();
  ~CongestionFeedbackMessageInterArrival();
  uint16 accumulated_number_of_lost_packets;
  // The set of received packets since the last feedback was sent, along with
  // their arrival times.
  TimeMap received_packet_times;
};

struct NET_EXPORT_PRIVATE CongestionFeedbackMessageFixRate {
  CongestionFeedbackMessageFixRate();
  QuicBandwidth bitrate;
};

struct NET_EXPORT_PRIVATE QuicCongestionFeedbackFrame {
  QuicCongestionFeedbackFrame();
  ~QuicCongestionFeedbackFrame();

  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicCongestionFeedbackFrame& c);

  CongestionFeedbackType type;
  // This should really be a union, but since the inter arrival struct
  // is non-trivial, C++ prohibits it.
  CongestionFeedbackMessageTCP tcp;
  CongestionFeedbackMessageInterArrival inter_arrival;
  CongestionFeedbackMessageFixRate fix_rate;
};

struct NET_EXPORT_PRIVATE QuicRstStreamFrame {
  QuicRstStreamFrame() {}
  QuicRstStreamFrame(QuicStreamId stream_id, uint64 offset,
                     QuicErrorCode error_code)
      : stream_id(stream_id), offset(offset), error_code(error_code) {
    DCHECK_LE(error_code, std::numeric_limits<uint8>::max());
  }

  QuicStreamId stream_id;
  uint64 offset;
  QuicErrorCode error_code;
  std::string error_details;
};

struct NET_EXPORT_PRIVATE QuicConnectionCloseFrame {
  QuicErrorCode error_code;
  std::string error_details;
  QuicAckFrame ack_frame;
};

struct NET_EXPORT_PRIVATE QuicFrame {
  QuicFrame() {}
  explicit QuicFrame(QuicPaddingFrame* padding_frame)
      : type(PADDING_FRAME),
        padding_frame(padding_frame) {
  }
  explicit QuicFrame(QuicStreamFrame* stream_frame)
      : type(STREAM_FRAME),
        stream_frame(stream_frame) {
  }
  explicit QuicFrame(QuicAckFrame* frame)
      : type(ACK_FRAME),
        ack_frame(frame) {
  }
  explicit QuicFrame(QuicCongestionFeedbackFrame* frame)
      : type(CONGESTION_FEEDBACK_FRAME),
        congestion_feedback_frame(frame) {
  }
  explicit QuicFrame(QuicRstStreamFrame* frame)
      : type(RST_STREAM_FRAME),
        rst_stream_frame(frame) {
  }
  explicit QuicFrame(QuicConnectionCloseFrame* frame)
      : type(CONNECTION_CLOSE_FRAME),
        connection_close_frame(frame) {
  }

  QuicFrameType type;
  union {
    QuicPaddingFrame* padding_frame;
    QuicStreamFrame* stream_frame;
    QuicAckFrame* ack_frame;
    QuicCongestionFeedbackFrame* congestion_feedback_frame;
    QuicRstStreamFrame* rst_stream_frame;
    QuicConnectionCloseFrame* connection_close_frame;
  };
};

typedef std::vector<QuicFrame> QuicFrames;

struct NET_EXPORT_PRIVATE QuicFecData {
  QuicFecData();

  bool operator==(const QuicFecData& other) const;

  // The FEC group number is also the sequence number of the first
  // FEC protected packet.  The last protected packet's sequence number will
  // be one less than the sequence number of the FEC packet.
  QuicFecGroupNumber fec_group;
  // The last protected packet's sequence number will be one
  // less than the sequence number of the FEC packet.
  base::StringPiece redundancy;
};

struct NET_EXPORT_PRIVATE QuicPacketData {
  std::string data;
};

class NET_EXPORT_PRIVATE QuicData {
 public:
  QuicData(const char* buffer, size_t length)
      : buffer_(buffer),
        length_(length),
        owns_buffer_(false) { }

  QuicData(char* buffer, size_t length, bool owns_buffer)
      : buffer_(buffer),
        length_(length),
        owns_buffer_(owns_buffer) { }

  virtual ~QuicData();

  base::StringPiece AsStringPiece() const {
    return base::StringPiece(data(), length());
  }

  const char* data() const { return buffer_; }
  size_t length() const { return length_; }

 private:
  const char* buffer_;
  size_t length_;
  bool owns_buffer_;

  DISALLOW_COPY_AND_ASSIGN(QuicData);
};

class NET_EXPORT_PRIVATE QuicPacket : public QuicData {
 public:
  static QuicPacket* NewDataPacket(char* buffer,
                               size_t length,
                               bool owns_buffer) {
    return new QuicPacket(buffer, length, owns_buffer, false);
  }

  static QuicPacket* NewFecPacket(char* buffer,
                                  size_t length,
                                  bool owns_buffer) {
    return new QuicPacket(buffer, length, owns_buffer, true);
  }

  base::StringPiece FecProtectedData() const {
    return base::StringPiece(data() + kStartOfFecProtectedData,
                             length() - kStartOfFecProtectedData);
  }

  base::StringPiece AssociatedData() const {
    return base::StringPiece(data() + kStartOfHashData,
                             kStartOfEncryptedData - kStartOfHashData);
  }

  base::StringPiece Plaintext() const {
    return base::StringPiece(data() + kStartOfEncryptedData,
                             length() - kStartOfEncryptedData);
  }

  bool is_fec_packet() const { return is_fec_packet_; }

  char* mutable_data() { return buffer_; }

 private:
  QuicPacket(char* buffer, size_t length, bool owns_buffer, bool is_fec_packet)
      : QuicData(buffer, length, owns_buffer),
        buffer_(buffer),
        is_fec_packet_(is_fec_packet) { }

  char* buffer_;
  const bool is_fec_packet_;

  DISALLOW_COPY_AND_ASSIGN(QuicPacket);
};

class NET_EXPORT_PRIVATE QuicEncryptedPacket : public QuicData {
 public:
  QuicEncryptedPacket(const char* buffer, size_t length)
      : QuicData(buffer, length) { }

  QuicEncryptedPacket(char* buffer, size_t length, bool owns_buffer)
      : QuicData(buffer, length, owns_buffer) { }

  // By default, gtest prints the raw bytes of an object. The bool data
  // member (in the base class QuicData) causes this object to have padding
  // bytes, which causes the default gtest object printer to read
  // uninitialize memory. So we need to teach gtest how to print this object.
  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicEncryptedPacket& s);

  base::StringPiece AssociatedData() const {
    return base::StringPiece(data() + kStartOfHashData,
                             kStartOfEncryptedData - kStartOfHashData);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicEncryptedPacket);
};

// A struct for functions which consume data payloads and fins.
// The first member of the pair indicates bytes consumed.
// The second member of the pair indicates if an incoming fin was consumed.
struct QuicConsumedData {
  QuicConsumedData(size_t bytes_consumed, bool fin_consumed)
      : bytes_consumed(bytes_consumed),
        fin_consumed(fin_consumed) {
  }

  // By default, gtest prints the raw bytes of an object. The bool data
  // member causes this object to have padding bytes, which causes the
  // default gtest object printer to read uninitialize memory. So we need
  // to teach gtest how to print this object.
  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicConsumedData& s);

  size_t bytes_consumed;
  bool fin_consumed;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PROTOCOL_H_
