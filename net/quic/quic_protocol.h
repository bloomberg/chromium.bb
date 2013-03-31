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
typedef uint8 QuicPacketEntropyHash;
typedef uint32 QuicVersionTag;
typedef std::vector<QuicVersionTag> QuicVersionTagList;

// TODO(rch): Consider Quic specific names for these constants.
// Maximum size in bytes of a QUIC packet.
const QuicByteCount kMaxPacketSize = 1200;

// Maximum number of open streams per connection.
const size_t kDefaultMaxStreamsPerConnection = 100;

// Number of bytes reserved for guid in the packet header.
const size_t kQuicGuidSize = 8;
// Number of bytes reserved for public flags in the packet header.
const size_t kPublicFlagsSize = 1;
// Number of bytes reserved for version number in the packet header.
const size_t kQuicVersionSize = 4;
// Number of bytes reserved for sequence number in the packet header.
const size_t kSequenceNumberSize = 6;
// Number of bytes reserved for private flags in the packet header.
const size_t kPrivateFlagsSize = 1;
// Number of bytes reserved for FEC group in the packet header.
const size_t kFecGroupSize = 1;
// Number of bytes reserved for the nonce proof in public reset packet.
const size_t kPublicResetNonceSize = 8;

// Signifies that the QuicPacket will contain version of the protocol.
const bool kIncludeVersion = true;

// Size in bytes of the data or fec packet header.
NET_EXPORT_PRIVATE size_t GetPacketHeaderSize(bool include_version);
// Size in bytes of the public reset packet.
NET_EXPORT_PRIVATE size_t GetPublicResetPacketSize();

// Index of the first byte in a QUIC packet of FEC protected data.
NET_EXPORT_PRIVATE size_t GetStartOfFecProtectedData(bool include_version);
// Index of the first byte in a QUIC packet of encrypted data.
NET_EXPORT_PRIVATE size_t GetStartOfEncryptedData(bool include_version);
// Returns true if |version| is a supported protocol version.
NET_EXPORT_PRIVATE bool IsSupportedVersion(QuicVersionTag version);

// Index of the first byte in a QUIC packet which is used in hash calculation.
const size_t kStartOfHashData = 0;

// Limit on the delta between stream IDs.
const QuicStreamId kMaxStreamIdDelta = 100;

// Reserved ID for the crypto stream.
// TODO(rch): ensure that this is not usable by any other streams.
const QuicStreamId kCryptoStreamId = 1;

// Value which indicates this packet is not FEC protected.
const uint8 kNoFecOffset = 0xFF;

const int64 kDefaultTimeoutUs = 600000000;  // 10 minutes.

enum Retransmission {
  NOT_RETRANSMISSION = 0,
  IS_RETRANSMISSION = 1,
};

enum HasRetransmittableData {
  HAS_RETRANSMITTABLE_DATA = 0,
  NO_RETRANSMITTABLE_DATA = 1,
};

enum QuicFrameType {
  PADDING_FRAME = 0,
  STREAM_FRAME,
  ACK_FRAME,
  CONGESTION_FEEDBACK_FRAME,
  RST_STREAM_FRAME,
  CONNECTION_CLOSE_FRAME,
  GOAWAY_FRAME,
  NUM_FRAME_TYPES
};

enum QuicPacketPublicFlags {
  PACKET_PUBLIC_FLAGS_NONE = 0,
  PACKET_PUBLIC_FLAGS_VERSION = 1 << 0,  // Packet header contains version info.
  PACKET_PUBLIC_FLAGS_RST = 1 << 1,  // Packet is a public reset packet.
  PACKET_PUBLIC_FLAGS_MAX = (1 << 2) - 1  // All bits set.
};

enum QuicPacketPrivateFlags {
  PACKET_PRIVATE_FLAGS_NONE = 0,
  PACKET_PRIVATE_FLAGS_FEC = 1 << 0,  // Payload is FEC as opposed to frames.
  PACKET_PRIVATE_FLAGS_ENTROPY = 1 << 1,
  PACKET_PRIVATE_FLAGS_FEC_ENTROPY = 1 << 2,
  PACKET_PRIVATE_FLAGS_MAX = (1 << 3) - 1  // All bits set.
};

enum QuicRstStreamErrorCode {
  QUIC_STREAM_NO_ERROR = 0,

  // There was some server error which halted stream processing.
  QUIC_SERVER_ERROR_PROCESSING_STREAM,
  // We got two fin or reset offsets which did not match.
  QUIC_MULTIPLE_TERMINATION_OFFSETS,
  // We got bad payload and can not respond to it at the protocol level.
  QUIC_BAD_APPLICATION_PAYLOAD,
  // Stream closed due to connection error. No reset frame is sent when this
  // happens.
  QUIC_STREAM_CONNECTION_ERROR,
  // GoAway frame sent. No more stream can be created.
  QUIC_STREAM_PEER_GOING_AWAY,

  // No error. Used as bound while iterating.
  QUIC_STREAM_LAST_ERROR,
};

enum QuicErrorCode {
  QUIC_NO_ERROR = 0,

  // Connection has reached an invalid state.
  QUIC_INTERNAL_ERROR,
  // There were data frames after the a fin or reset.
  QUIC_STREAM_DATA_AFTER_TERMINATION,
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
  // GoAway data is malformed.
  QUIC_INVALID_GOAWAY_DATA,
  // Ack data is malformed.
  QUIC_INVALID_ACK_DATA,
  // Version negotiation packet is malformed.
  QUIC_INVALID_VERSION_NEGOTIATION_PACKET,
  // There was an error decrypting.
  QUIC_DECRYPTION_FAILURE,
  // There was an error encrypting.
  QUIC_ENCRYPTION_FAILURE,
  // The packet exceeded kMaxPacketSize.
  QUIC_PACKET_TOO_LARGE,
  // Data was sent for a stream which did not exist.
  QUIC_PACKET_FOR_NONEXISTENT_STREAM,
  // The peer is going away.  May be a client or server.
  QUIC_PEER_GOING_AWAY,
  // A stream ID was invalid.
  QUIC_INVALID_STREAM_ID,
  // Too many streams already open.
  QUIC_TOO_MANY_OPEN_STREAMS,
  // Received public reset for this connection.
  QUIC_PUBLIC_RESET,
  // Invalid protocol version
  QUIC_INVALID_VERSION,

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
  // A crypto message was received that contained a parameter with too few
  // values.
  QUIC_CRYPTO_MESSAGE_INDEX_NOT_FOUND,
  // An internal error occured in crypto processing.
  QUIC_CRYPTO_INTERNAL_ERROR,
  // A crypto handshake message specified an unsupported version.
  QUIC_VERSION_NOT_SUPPORTED,
  // There was no intersection between the crypto primitives supported by the
  // peer and ourselves.
  QUIC_CRYPTO_NO_SUPPORT,

  // No error. Used as bound while iterating.
  QUIC_LAST_ERROR,
};

// Version and Crypto tags are written to the wire with a big-endian
// representation of the name of the tag.  For example
// the client hello tag (CHLO) will be written as the
// following 4 bytes: 'C' 'H' 'L' 'O'.  Since it is
// stored in memory as a little endian uint32, we need
// to reverse the order of the bytes.
#define MAKE_TAG(a, b, c, d) ((d << 24) + (c << 16) + (b << 8) + a)

const QuicVersionTag kUnsupportedVersion = -1;
const QuicVersionTag kQuicVersion1 = MAKE_TAG('Q', '1', '.', '0');

struct NET_EXPORT_PRIVATE QuicPacketPublicHeader {
  QuicPacketPublicHeader();
  explicit QuicPacketPublicHeader(const QuicPacketPublicHeader& other);
  ~QuicPacketPublicHeader();

  QuicPacketPublicHeader& operator=(const QuicPacketPublicHeader& other);

  // Universal header. All QuicPacket headers will have a guid and public flags.
  QuicGuid guid;
  bool reset_flag;
  bool version_flag;
  QuicVersionTagList versions;
};

// Header for Data or FEC packets.
struct NET_EXPORT_PRIVATE QuicPacketHeader {
  QuicPacketHeader();
  explicit QuicPacketHeader(const QuicPacketPublicHeader& header);

  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicPacketHeader& s);

  QuicPacketPublicHeader public_header;
  bool fec_flag;
  bool fec_entropy_flag;
  bool entropy_flag;
  QuicPacketEntropyHash entropy_hash;
  QuicPacketSequenceNumber packet_sequence_number;
  QuicFecGroupNumber fec_group;
};

struct NET_EXPORT_PRIVATE QuicPublicResetPacket {
  QuicPublicResetPacket() {}
  explicit QuicPublicResetPacket(const QuicPacketPublicHeader& header)
      : public_header(header) {}
  QuicPacketPublicHeader public_header;
  QuicPacketSequenceNumber rejected_sequence_number;
  QuicPublicResetNonceProof nonce_proof;
};

enum QuicVersionNegotiationState {
  START_NEGOTIATION = 0,
  SENT_NEGOTIATION_PACKET,
  NEGOTIATED_VERSION
};

typedef QuicPacketPublicHeader QuicVersionNegotiationPacket;

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
typedef std::set<QuicPacketSequenceNumber> SequenceNumberSet;
// TODO(pwestin): Add a way to enforce the max size of this map.
typedef std::map<QuicPacketSequenceNumber, QuicTime> TimeMap;

struct NET_EXPORT_PRIVATE ReceivedPacketInfo {
  ReceivedPacketInfo();
  ~ReceivedPacketInfo();
  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const ReceivedPacketInfo& s);

  // Entropy hash of all packets up to largest observed not including missing
  // packets.
  QuicPacketEntropyHash entropy_hash;

  // The highest packet sequence number we've observed from the peer.
  //
  // In general, this should be the largest packet number we've received.  In
  // the case of truncated acks, we may have to advertise a lower "upper bound"
  // than largest received, to avoid implicitly acking missing packets that
  // don't fit in the missing packet list due to size limitations.  In this
  // case, largest_observed may be a packet which is also in the missing packets
  // list.
  QuicPacketSequenceNumber largest_observed;

  // Time elapsed since largest_observed was received until this Ack frame was
  // sent.
  QuicTime::Delta delta_time_largest_observed;

  // TODO(satyamshekhar): Can be optimized using an interval set like data
  // structure.
  // The set of packets which we're expecting and have not received.
  SequenceNumberSet missing_packets;
};

// True if the sequence number is greater than largest_observed or is listed
// as missing.
// Always returns false for sequence numbers less than least_unacked.
bool NET_EXPORT_PRIVATE IsAwaitingPacket(
    const ReceivedPacketInfo& received_info,
    QuicPacketSequenceNumber sequence_number);

// Inserts missing packets between [lower, higher).
void NET_EXPORT_PRIVATE InsertMissingPacketsBetween(
    ReceivedPacketInfo* received_info,
    QuicPacketSequenceNumber lower,
    QuicPacketSequenceNumber higher);

struct NET_EXPORT_PRIVATE SentPacketInfo {
  SentPacketInfo();
  ~SentPacketInfo();
  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const SentPacketInfo& s);

  // Entropy hash of all packets up to, but not including, the least unacked
  // packet.
  QuicPacketEntropyHash entropy_hash;
  // The lowest packet we've sent which is unacked, and we expect an ack for.
  QuicPacketSequenceNumber least_unacked;
};

struct NET_EXPORT_PRIVATE QuicAckFrame {
  QuicAckFrame() {}
  // Testing convenience method to construct a QuicAckFrame with all packets
  // from least_unacked to largest_observed acked.
  QuicAckFrame(QuicPacketSequenceNumber largest_observed,
               QuicTime largest_observed_receive_time,
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
  QuicRstStreamFrame(QuicStreamId stream_id, QuicRstStreamErrorCode error_code)
      : stream_id(stream_id), error_code(error_code) {
    DCHECK_LE(error_code, std::numeric_limits<uint8>::max());
  }

  QuicStreamId stream_id;
  QuicRstStreamErrorCode error_code;
  std::string error_details;
};

struct NET_EXPORT_PRIVATE QuicConnectionCloseFrame {
  QuicErrorCode error_code;
  std::string error_details;
  QuicAckFrame ack_frame;
};

struct NET_EXPORT_PRIVATE QuicGoAwayFrame {
  QuicGoAwayFrame() {}
  QuicGoAwayFrame(QuicErrorCode error_code,
                  QuicStreamId last_good_stream_id,
                  const std::string& reason);

  QuicErrorCode error_code;
  QuicStreamId last_good_stream_id;
  std::string reason_phrase;
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
  explicit QuicFrame(QuicGoAwayFrame* frame)
      : type(GOAWAY_FRAME),
        goaway_frame(frame) {
  }

  QuicFrameType type;
  union {
    QuicPaddingFrame* padding_frame;
    QuicStreamFrame* stream_frame;
    QuicAckFrame* ack_frame;
    QuicCongestionFeedbackFrame* congestion_feedback_frame;
    QuicRstStreamFrame* rst_stream_frame;
    QuicConnectionCloseFrame* connection_close_frame;
    QuicGoAwayFrame* goaway_frame;
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
        owns_buffer_(false) {}

  QuicData(char* buffer, size_t length, bool owns_buffer)
      : buffer_(buffer),
        length_(length),
        owns_buffer_(owns_buffer) {}

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
                                   bool owns_buffer,
                                   bool includes_version) {
    return new QuicPacket(buffer, length, owns_buffer, includes_version, false);
  }

  static QuicPacket* NewFecPacket(char* buffer,
                                  size_t length,
                                  bool owns_buffer,
                                  bool includes_version) {
    return new QuicPacket(buffer, length, owns_buffer, includes_version, true);
  }

  base::StringPiece FecProtectedData() const;
  base::StringPiece AssociatedData() const;
  base::StringPiece BeforePlaintext() const;
  base::StringPiece Plaintext() const;

  bool is_fec_packet() const { return is_fec_packet_; }

  bool includes_version() const { return includes_version_; }

  char* mutable_data() { return buffer_; }

 private:
  QuicPacket(char* buffer,
             size_t length,
             bool owns_buffer,
             bool includes_version,
             bool is_fec_packet)
      : QuicData(buffer, length, owns_buffer),
        buffer_(buffer),
        is_fec_packet_(is_fec_packet),
        includes_version_(includes_version) {}

  char* buffer_;
  const bool is_fec_packet_;
  const bool includes_version_;

  DISALLOW_COPY_AND_ASSIGN(QuicPacket);
};

class NET_EXPORT_PRIVATE QuicEncryptedPacket : public QuicData {
 public:
  QuicEncryptedPacket(const char* buffer, size_t length)
      : QuicData(buffer, length) {}

  QuicEncryptedPacket(char* buffer, size_t length, bool owns_buffer)
      : QuicData(buffer, length, owns_buffer) {}

  // By default, gtest prints the raw bytes of an object. The bool data
  // member (in the base class QuicData) causes this object to have padding
  // bytes, which causes the default gtest object printer to read
  // uninitialize memory. So we need to teach gtest how to print this object.
  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicEncryptedPacket& s);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicEncryptedPacket);
};

class NET_EXPORT_PRIVATE RetransmittableFrames {
 public:
  RetransmittableFrames();
  ~RetransmittableFrames();

  // Allocates a local copy of the referenced StringPiece has QuicStreamFrame
  // use it.
  // Takes ownership of |stream_frame|.
  const QuicFrame& AddStreamFrame(QuicStreamFrame* stream_frame);
  // Takes ownership of the frame inside |frame|.
  const QuicFrame& AddNonStreamFrame(const QuicFrame& frame);
  const QuicFrames& frames() const { return frames_; }

 private:
  QuicFrames frames_;
  // Data referenced by the StringPiece of a QuicStreamFrame.
  std::vector<std::string*> stream_data_;

  DISALLOW_COPY_AND_ASSIGN(RetransmittableFrames);
};

struct NET_EXPORT_PRIVATE SerializedPacket {
  SerializedPacket(QuicPacketSequenceNumber sequence_number,
                   QuicPacket* packet,
                   QuicPacketEntropyHash entropy_hash,
                   RetransmittableFrames* retransmittable_frames)
      : sequence_number(sequence_number),
        packet(packet),
        entropy_hash(entropy_hash),
        retransmittable_frames(retransmittable_frames) {}

  QuicPacketSequenceNumber sequence_number;
  QuicPacket* packet;
  QuicPacketEntropyHash entropy_hash;
  RetransmittableFrames* retransmittable_frames;
};

// A struct for functions which consume data payloads and fins.
// The first member of the pair indicates bytes consumed.
// The second member of the pair indicates if an incoming fin was consumed.
struct QuicConsumedData {
  QuicConsumedData(size_t bytes_consumed, bool fin_consumed)
      : bytes_consumed(bytes_consumed),
        fin_consumed(fin_consumed) {}

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
