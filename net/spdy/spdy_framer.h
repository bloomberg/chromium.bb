// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_FRAMER_H_
#define NET_SPDY_SPDY_FRAMER_H_

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_byteorder.h"
#include "net/base/net_export.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_protocol.h"

typedef struct z_stream_s z_stream;  // Forward declaration for zlib.

namespace net {

class HttpProxyClientSocketPoolTest;
class HttpNetworkLayer;
class HttpNetworkTransactionTest;
class SpdyHttpStreamTest;
class SpdyNetworkTransactionTest;
class SpdyProxyClientSocketTest;
class SpdySessionTest;
class SpdyStreamTest;
class SpdyWebSocketStreamTest;
class WebSocketJobTest;

class SpdyFramer;
class SpdyFrameBuilder;
class SpdyFramerTest;

namespace test {

class TestSpdyVisitor;

}  // namespace test

// A datastructure for holding a set of headers from either a
// SYN_STREAM or SYN_REPLY frame.
typedef std::map<std::string, std::string> SpdyHeaderBlock;

// A datastructure for holding the ID and flag fields for SETTINGS.
// Conveniently handles converstion to/from wire format.
class NET_EXPORT_PRIVATE SettingsFlagsAndId {
 public:
  static SettingsFlagsAndId FromWireFormat(int version, uint32 wire);

  SettingsFlagsAndId() : flags_(0), id_(0) {}

  // TODO(hkhalil): restrict to enums instead of free-form ints.
  SettingsFlagsAndId(uint8 flags, uint32 id);

  uint32 GetWireFormat(int version) const;

  uint32 id() const { return id_; }
  uint8 flags() const { return flags_; }

 private:
  static void ConvertFlagsAndIdForSpdy2(uint32* val);

  uint8 flags_;
  uint32 id_;
};

// SettingsMap has unique (flags, value) pair for given SpdySettingsIds ID.
typedef std::pair<SpdySettingsFlags, uint32> SettingsFlagsAndValue;
typedef std::map<SpdySettingsIds, SettingsFlagsAndValue> SettingsMap;

// A datastrcture for holding the contents of a CREDENTIAL frame.
// TODO(hkhalil): Remove, use SpdyCredentialIR instead.
struct NET_EXPORT_PRIVATE SpdyCredential {
  SpdyCredential();
  ~SpdyCredential();

  uint16 slot;
  std::vector<std::string> certs;
  std::string proof;
};

// Scratch space necessary for processing SETTINGS frames.
struct NET_EXPORT_PRIVATE SpdySettingsScratch {
  SpdySettingsScratch() { Reset(); }

  void Reset() {
    setting_buf_len = 0;
    last_setting_id = 0;
  }

  // Buffer contains up to one complete key/value pair.
  char setting_buf[8];

  // The amount of the buffer that is filled with valid data.
  size_t setting_buf_len;

  // The ID of the last setting that was processed in the current SETTINGS
  // frame. Used for detecting out-of-order or duplicate keys within a settings
  // frame. Set to 0 before first key/value pair is processed.
  uint32 last_setting_id;
};

// SpdyFramerVisitorInterface is a set of callbacks for the SpdyFramer.
// Implement this interface to receive event callbacks as frames are
// decoded from the framer.
//
// Control frames that contain SPDY header blocks (SYN_STREAM, SYN_REPLY, and
// HEADER) are processed in fashion that allows the decompressed header block
// to be delivered in chunks to the visitor. The following steps are followed:
//   1. OnSynStream, OnSynReply or OnHeaders is called.
//   2. Repeated: OnControlFrameHeaderData is called with chunks of the
//      decompressed header block. In each call the len parameter is greater
//      than zero.
//   3. OnControlFrameHeaderData is called with len set to zero, indicating
//      that the full header block has been delivered for the control frame.
// During step 2 the visitor may return false, indicating that the chunk of
// header data could not be handled by the visitor (typically this indicates
// resource exhaustion). If this occurs the framer will discontinue
// delivering chunks to the visitor, set a SPDY_CONTROL_PAYLOAD_TOO_LARGE
// error, and clean up appropriately. Note that this will cause the header
// decompressor to lose synchronization with the sender's header compressor,
// making the SPDY session unusable for future work. The visitor's OnError
// function should deal with this condition by closing the SPDY connection.
class NET_EXPORT_PRIVATE SpdyFramerVisitorInterface {
 public:
  virtual ~SpdyFramerVisitorInterface() {}

  // Called if an error is detected in the SpdyFrame protocol.
  virtual void OnError(SpdyFramer* framer) = 0;

  // Called when a SYN_STREAM frame is received.
  // Note that header block data is not included. See
  // OnControlFrameHeaderData().
  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           uint8 credential_slot,
                           bool fin,
                           bool unidirectional) = 0;

  // Called when a SYN_REPLY frame is received.
  // Note that header block data is not included. See
  // OnControlFrameHeaderData().
  virtual void OnSynReply(SpdyStreamId stream_id, bool fin) = 0;

  // Called when a HEADERS frame is received.
  // Note that header block data is not included. See
  // OnControlFrameHeaderData().
  virtual void OnHeaders(SpdyStreamId stream_id, bool fin) = 0;

  // Called when a chunk of header data is available. This is called
  // after OnSynStream, OnSynReply or OnHeaders().
  // |stream_id| The stream receiving the header data.
  // |header_data| A buffer containing the header data chunk received.
  // |len| The length of the header data buffer. A length of zero indicates
  //       that the header data block has been completely sent.
  // When this function returns true the visitor indicates that it accepted
  // all of the data. Returning false indicates that that an unrecoverable
  // error has occurred, such as bad header data or resource exhaustion.
  virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) = 0;

  // Called when a chunk of payload data for a credential frame is available.
  // |header_data| A buffer containing the header data chunk received.
  // |len| The length of the header data buffer. A length of zero indicates
  //       that the header data block has been completely sent.
  // When this function returns true the visitor indicates that it accepted
  // all of the data. Returning false indicates that that an unrecoverable
  // error has occurred, such as bad header data or resource exhaustion.
  virtual bool OnCredentialFrameData(const char* credential_data,
                                     size_t len) = 0;

  // Called when a data frame header is received. The frame's data
  // payload will be provided via subsequent calls to
  // OnStreamFrameData().
  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) = 0;

  // Called when data is received.
  // |stream_id| The stream receiving data.
  // |data| A buffer containing the data received.
  // |len| The length of the data buffer.
  // When the other side has finished sending data on this stream,
  // this method will be called with a zero-length buffer.
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 SpdyDataFlags flags) = 0;

  // Called when a complete setting within a SETTINGS frame has been parsed and
  // validated.
  virtual void OnSetting(SpdySettingsIds id, uint8 flags, uint32 value) = 0;

  // Called when a PING frame has been parsed.
  virtual void OnPing(uint32 unique_id) = 0;

  // Called when a RST_STREAM frame has been parsed.
  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) = 0;

  // Called when a GOAWAY frame has been parsed.
  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) = 0;

  // Called when a WINDOW_UPDATE frame has been parsed.
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) = 0;

  // Called after a control frame has been compressed to allow the visitor
  // to record compression statistics.
  //
  // TODO(akalin): Upstream this function.
  virtual void OnSynStreamCompressed(
      size_t uncompressed_size,
      size_t compressed_size) = 0;
};

// Optionally, and in addition to SpdyFramerVisitorInterface, a class supporting
// SpdyFramerDebugVisitorInterface may be used in conjunction with SpdyFramer in
// order to extract debug/internal information about the SpdyFramer as it
// operates.
//
// Most SPDY implementations need not bother with this interface at all.
class SpdyFramerDebugVisitorInterface {
 public:
  virtual ~SpdyFramerDebugVisitorInterface() {}

  // Called after compressing header blocks.
  // Provides uncompressed and compressed sizes.
  virtual void OnCompressedHeaderBlock(size_t uncompressed_len,
                                       size_t compressed_len) {}

  // Called when decompressing header blocks.
  // Provides uncompressed and compressed sizes.
  // Called once per incremental decompression. That is to say, if a header
  // block is decompressed in four chunks, this will result in four calls to
  // OnDecompressedHeaderBlock() interleaved with four calls to
  // OnControlFrameHeaderData(). Note that uncompressed_len may be 0 in some
  // valid cases, even though compressed_len is nonzero.
  virtual void OnDecompressedHeaderBlock(size_t uncompressed_len,
                                         size_t compressed_len) {}
};

class NET_EXPORT_PRIVATE SpdyFramer {
 public:
  // SPDY states.
  // TODO(mbelshe): Can we move these into the implementation
  //                and avoid exposing through the header.  (Needed for test)
  enum SpdyState {
    SPDY_ERROR,
    SPDY_RESET,
    SPDY_AUTO_RESET,
    SPDY_READING_COMMON_HEADER,
    SPDY_CONTROL_FRAME_PAYLOAD,
    SPDY_IGNORE_REMAINING_PAYLOAD,
    SPDY_FORWARD_STREAM_FRAME,
    SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK,
    SPDY_CONTROL_FRAME_HEADER_BLOCK,
    SPDY_CREDENTIAL_FRAME_PAYLOAD,
    SPDY_SETTINGS_FRAME_PAYLOAD,
  };

  // SPDY error codes.
  enum SpdyError {
    SPDY_NO_ERROR,
    SPDY_INVALID_CONTROL_FRAME,        // Control frame is mal-formatted.
    SPDY_CONTROL_PAYLOAD_TOO_LARGE,    // Control frame payload was too large.
    SPDY_ZLIB_INIT_FAILURE,            // The Zlib library could not initialize.
    SPDY_UNSUPPORTED_VERSION,          // Control frame has unsupported version.
    SPDY_DECOMPRESS_FAILURE,           // There was an error decompressing.
    SPDY_COMPRESS_FAILURE,             // There was an error compressing.
    SPDY_CREDENTIAL_FRAME_CORRUPT,     // CREDENTIAL frame could not be parsed.
    SPDY_INVALID_DATA_FRAME_FLAGS,     // Data frame has invalid flags.
    SPDY_INVALID_CONTROL_FRAME_FLAGS,  // Control frame has invalid flags.

    LAST_ERROR,  // Must be the last entry in the enum.
  };

  // The minimum supported SPDY version that SpdyFramer can speak.
  static const int kMinSpdyVersion;

  // The maximum supported SPDY version that SpdyFramer can speak.
  static const int kMaxSpdyVersion;

  // Constant for invalid (or unknown) stream IDs.
  static const SpdyStreamId kInvalidStream;

  // The maximum size of header data chunks delivered to the framer visitor
  // through OnControlFrameHeaderData. (It is exposed here for unit test
  // purposes.)
  static const size_t kHeaderDataChunkMaxSize;

  // Serializes a SpdyHeaderBlock.
  static void WriteHeaderBlock(SpdyFrameBuilder* frame,
                               const int spdy_version,
                               const SpdyHeaderBlock* headers);

  // Retrieve serialized length of SpdyHeaderBlock.
  // TODO(hkhalil): Remove, or move to quic code.
  static size_t GetSerializedLength(const int spdy_version,
                                    const SpdyHeaderBlock* headers);

  // Create a new Framer, provided a SPDY version.
  explicit SpdyFramer(int version);
  virtual ~SpdyFramer();

  // Set callbacks to be called from the framer.  A visitor must be set, or
  // else the framer will likely crash.  It is acceptable for the visitor
  // to do nothing.  If this is called multiple times, only the last visitor
  // will be used.
  void set_visitor(SpdyFramerVisitorInterface* visitor) {
    visitor_ = visitor;
  }

  // Set debug callbacks to be called from the framer. The debug visitor is
  // completely optional and need not be set in order for normal operation.
  // If this is called multiple times, only the last visitor will be used.
  void set_debug_visitor(SpdyFramerDebugVisitorInterface* debug_visitor) {
    debug_visitor_ = debug_visitor;
  }

  // Pass data into the framer for parsing.
  // Returns the number of bytes consumed. It is safe to pass more bytes in
  // than may be consumed.
  size_t ProcessInput(const char* data, size_t len);

  // Resets the framer state after a frame has been successfully decoded.
  // TODO(mbelshe): can we make this private?
  void Reset();

  // Check the state of the framer.
  SpdyError error_code() const { return error_code_; }
  SpdyState state() const { return state_; }
  bool HasError() const { return state_ == SPDY_ERROR; }

  // Given a buffer containing a decompressed header block in SPDY
  // serialized format, parse out a SpdyHeaderBlock, putting the results
  // in the given header block.
  // Returns number of bytes consumed if successfully parsed, 0 otherwise.
  size_t ParseHeaderBlockInBuffer(const char* header_data,
                                size_t header_length,
                                SpdyHeaderBlock* block) const;

  // Creates and serializes a SYN_STREAM frame.
  // |stream_id| is the id for this stream.
  // |associated_stream_id| is the associated stream id for this stream.
  // |priority| is the priority (GetHighestPriority()-GetLowestPriority) for
  //    this stream.
  // |credential_slot| is the CREDENTIAL slot to be used for this request.
  // |flags| is the flags to use with the data.
  //    To mark this frame as the last frame, enable CONTROL_FLAG_FIN.
  // |compressed| specifies whether the frame should be compressed.
  // |headers| is the header block to include in the frame.
  SpdyFrame* CreateSynStream(SpdyStreamId stream_id,
                             SpdyStreamId associated_stream_id,
                             SpdyPriority priority,
                             uint8 credential_slot,
                             SpdyControlFlags flags,
                             bool compressed,
                             const SpdyHeaderBlock* headers);
  SpdySerializedFrame* SerializeSynStream(const SpdySynStreamIR& syn_stream);

  // Create a SYN_REPLY SpdyFrame.
  // |stream_id| is the stream for this frame.
  // |flags| is the flags to use with the data.
  //    To mark this frame as the last frame, enable CONTROL_FLAG_FIN.
  // |compressed| specifies whether the frame should be compressed.
  // |headers| is the header block to include in the frame.
  SpdyFrame* CreateSynReply(SpdyStreamId stream_id,
                            SpdyControlFlags flags,
                            bool compressed,
                            const SpdyHeaderBlock* headers);
  SpdySerializedFrame* SerializeSynReply(const SpdySynReplyIR& syn_reply);

  SpdyFrame* CreateRstStream(SpdyStreamId stream_id,
                             SpdyRstStreamStatus status) const;
  SpdySerializedFrame* SerializeRstStream(
      const SpdyRstStreamIR& rst_stream) const;

  // Creates and serializes a SETTINGS frame. The SETTINGS frame is
  // used to communicate name/value pairs relevant to the communication channel.
  SpdyFrame* CreateSettings(const SettingsMap& values) const;
  SpdySerializedFrame* SerializeSettings(const SpdySettingsIR& settings) const;

  // Creates and serializes a PING frame. The unique_id is used to
  // identify the ping request/response.
  SpdyFrame* CreatePingFrame(uint32 unique_id) const;
  SpdySerializedFrame* SerializePing(const SpdyPingIR& ping) const;

  // Creates and serializes a GOAWAY frame. The GOAWAY frame is used
  // prior to the shutting down of the TCP connection, and includes the
  // stream_id of the last stream the sender of the frame is willing to process
  // to completion.
  SpdyFrame* CreateGoAway(SpdyStreamId last_accepted_stream_id,
                          SpdyGoAwayStatus status) const;
  SpdySerializedFrame* SerializeGoAway(const SpdyGoAwayIR& goaway) const;

  // Creates and serializes a HEADERS frame. The HEADERS frame is used
  // for sending additional headers outside of a SYN_STREAM/SYN_REPLY. The
  // arguments are the same as for CreateSynReply.
  SpdyFrame* CreateHeaders(SpdyStreamId stream_id,
                           SpdyControlFlags flags,
                           bool compressed,
                           const SpdyHeaderBlock* headers);
  SpdySerializedFrame* SerializeHeaders(const SpdyHeadersIR& headers);

  // Creates and serializes a WINDOW_UPDATE frame. The WINDOW_UPDATE
  // frame is used to implement per stream flow control in SPDY.
  SpdyFrame* CreateWindowUpdate(
      SpdyStreamId stream_id,
      uint32 delta_window_size) const;
  SpdySerializedFrame* SerializeWindowUpdate(
      const SpdyWindowUpdateIR& window_update) const;

  // Creates and serializes a CREDENTIAL frame.  The CREDENTIAL
  // frame is used to send a client certificate to the server when
  // request more than one origin are sent over the same SPDY session.
  SpdyFrame* CreateCredentialFrame(const SpdyCredential& credential) const;
  SpdySerializedFrame* SerializeCredential(
      const SpdyCredentialIR& credential) const;

  // Given a CREDENTIAL frame's payload, extract the credential.
  // Returns true on successful parse, false otherwise.
  // TODO(hkhalil): Implement CREDENTIAL frame parsing in SpdyFramer
  // and eliminate this method.
  static bool ParseCredentialData(const char* data, size_t len,
                                  SpdyCredential* credential);

  // Create a data frame.
  // |stream_id| is the stream  for this frame
  // |data| is the data to be included in the frame.
  // |len| is the length of the data
  // |flags| is the flags to use with the data.
  //    To mark this frame as the last data frame, enable DATA_FLAG_FIN.
  SpdyFrame* CreateDataFrame(SpdyStreamId stream_id, const char* data,
                             uint32 len, SpdyDataFlags flags) const;
  SpdySerializedFrame* SerializeData(const SpdyDataIR& data) const;
  // Serializes just the data frame header, excluding actual data payload.
  SpdySerializedFrame* SerializeDataFrameHeader(const SpdyDataIR& data) const;

  // NOTES about frame compression.
  // We want spdy to compress headers across the entire session.  As long as
  // the session is over TCP, frames are sent serially.  The client & server
  // can each compress frames in the same order and then compress them in that
  // order, and the remote can do the reverse.  However, we ultimately want
  // the creation of frames to be less sensitive to order so that they can be
  // placed over a UDP based protocol and yet still benefit from some
  // compression.  We don't know of any good compression protocol which does
  // not build its state in a serial (stream based) manner....  For now, we're
  // using zlib anyway.

  // Compresses a SpdyFrame.
  // On success, returns a new SpdyFrame with the payload compressed.
  // Compression state is maintained as part of the SpdyFramer.
  // Returned frame must be freed with "delete".
  // On failure, returns NULL.
  SpdyFrame* CompressFrame(const SpdyFrame& frame);

  // For ease of testing and experimentation we can tweak compression on/off.
  void set_enable_compression(bool value) {
    enable_compression_ = value;
  }

  // Used only in log messages.
  void set_display_protocol(const std::string& protocol) {
    display_protocol_ = protocol;
  }

  // Returns the (minimum) size of frames (sans variable-length portions).
  size_t GetDataFrameMinimumSize() const;
  size_t GetControlFrameHeaderSize() const;
  size_t GetSynStreamMinimumSize() const;
  size_t GetSynReplyMinimumSize() const;
  size_t GetRstStreamSize() const;
  size_t GetSettingsMinimumSize() const;
  size_t GetPingSize() const;
  size_t GetGoAwaySize() const;
  size_t GetHeadersMinimumSize() const;
  size_t GetWindowUpdateSize() const;
  size_t GetCredentialMinimumSize() const;

  // For debugging.
  static const char* StateToString(int state);
  static const char* ErrorCodeToString(int error_code);
  static const char* StatusCodeToString(int status_code);
  static const char* ControlTypeToString(SpdyControlType type);

  int protocol_version() const { return spdy_version_; }

  bool probable_http_response() const { return probable_http_response_; }

  SpdyPriority GetLowestPriority() const { return spdy_version_ < 3 ? 3 : 7; }
  SpdyPriority GetHighestPriority() const { return 0; }

 protected:
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, BasicCompression);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, ControlFrameSizesAreValidated);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, HeaderCompression);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, DecompressUncompressedFrame);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, ExpandBuffer_HeapSmash);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, HugeHeaderBlock);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, UnclosedStreamDataCompressors);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest,
                           UnclosedStreamDataCompressorsOneByteAtATime);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest,
                           UncompressLargerThanFrameBufferInitialSize);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, ReadLargeSettingsFrame);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest,
                           ReadLargeSettingsFrameInSmallChunks);
  friend class net::HttpNetworkLayer;  // This is temporary for the server.
  friend class net::HttpNetworkTransactionTest;
  friend class net::HttpProxyClientSocketPoolTest;
  friend class net::SpdyHttpStreamTest;
  friend class net::SpdyNetworkTransactionTest;
  friend class net::SpdyProxyClientSocketTest;
  friend class net::SpdySessionTest;
  friend class net::SpdyStreamTest;
  friend class net::SpdyWebSocketStreamTest;
  friend class net::WebSocketJobTest;
  friend class test::TestSpdyVisitor;

 private:
  // Internal breakouts from ProcessInput. Each returns the number of bytes
  // consumed from the data.
  size_t ProcessCommonHeader(const char* data, size_t len);
  size_t ProcessControlFramePayload(const char* data, size_t len);
  size_t ProcessCredentialFramePayload(const char* data, size_t len);
  size_t ProcessControlFrameBeforeHeaderBlock(const char* data, size_t len);
  size_t ProcessControlFrameHeaderBlock(const char* data, size_t len);
  size_t ProcessSettingsFramePayload(const char* data, size_t len);
  size_t ProcessDataFramePayload(const char* data, size_t len);

  // Helpers for above internal breakouts from ProcessInput.
  void ProcessControlFrameHeader();
  bool ProcessSetting(const char* data);  // Always passed exactly 8 bytes.

  // Retrieve serialized length of SpdyHeaderBlock. If compression is enabled, a
  // maximum estimate is returned.
  size_t GetSerializedLength(const SpdyHeaderBlock& headers);

  // Get (and lazily initialize) the ZLib state.
  z_stream* GetHeaderCompressor();
  z_stream* GetHeaderDecompressor();

  // Deliver the given control frame's compressed headers block to the visitor
  // in decompressed form, in chunks. Returns true if the visitor has
  // accepted all of the chunks.
  bool IncrementallyDecompressControlFrameHeaderData(
      SpdyStreamId stream_id,
      const char* data,
      size_t len);

  // Deliver the given control frame's uncompressed headers block to the
  // visitor in chunks. Returns true if the visitor has accepted all of the
  // chunks.
  bool IncrementallyDeliverControlFrameHeaderData(SpdyStreamId stream_id,
                                                  const char* data,
                                                  size_t len);

  // Utility to copy the given data block to the current frame buffer, up
  // to the given maximum number of bytes, and update the buffer
  // data (pointer and length). Returns the number of bytes
  // read, and:
  //   *data is advanced the number of bytes read.
  //   *len is reduced by the number of bytes read.
  size_t UpdateCurrentFrameBuffer(const char** data, size_t* len,
                                  size_t max_bytes);

  void WriteHeaderBlockToZ(const SpdyHeaderBlock* headers,
                           z_stream* out) const;

  void SerializeNameValueBlockWithoutCompression(
      SpdyFrameBuilder* builder,
      const SpdyFrameWithNameValueBlockIR& frame) const;

  // Compresses automatically according to enable_compression_.
  void SerializeNameValueBlock(
      SpdyFrameBuilder* builder,
      const SpdyFrameWithNameValueBlockIR& frame);

  // Set the error code and moves the framer into the error state.
  void set_error(SpdyError error);

  size_t GoAwaySize() const;

  // The maximum size of the control frames that we support.
  // This limit is arbitrary. We can enforce it here or at the application
  // layer. We chose the framing layer, but this can be changed (or removed)
  // if necessary later down the line.
  size_t GetControlFrameBufferMaxSize() const {
     return (spdy_version_ == 2) ? 64 * 1024 : 16 * 1024 * 1024;
  }

  // The size of the control frame buffer.
  // Since this is only used for control frame headers, the maximum control
  // frame header size (SYN_STREAM) is sufficient; all remaining control
  // frame data is streamed to the visitor.
  static const size_t kControlFrameBufferSize;

  SpdyState state_;
  SpdyState previous_state_;
  SpdyError error_code_;
  size_t remaining_data_;

  // The number of bytes remaining to read from the current control frame's
  // payload.
  size_t remaining_control_payload_;

  // The number of bytes remaining to read from the current control frame's
  // headers. Note that header data blocks (for control types that have them)
  // are part of the frame's payload, and not the frame's headers.
  size_t remaining_control_header_;

  scoped_array<char> current_frame_buffer_;
  // Number of bytes read into the current_frame_buffer_.
  size_t current_frame_buffer_length_;

  // The type of the frame currently being read. Set to NUM_CONTROL_FRAME_TYPES
  // if currently processing a DATA frame.
  SpdyControlType current_frame_type_;

  // The flags field of the frame currently being read.
  uint8 current_frame_flags_;

  // The length field of the frame currently being read.
  uint32 current_frame_length_;

  // The stream ID field of the frame currently being read, if applicable.
  SpdyStreamId current_frame_stream_id_;

  // Scratch space for handling SETTINGS frames.
  // TODO(hkhalil): Unify memory for this scratch space with
  // current_frame_buffer_.
  SpdySettingsScratch settings_scratch_;

  bool enable_compression_;  // Controls all compression
  // SPDY header compressors.
  scoped_ptr<z_stream> header_compressor_;
  scoped_ptr<z_stream> header_decompressor_;

  SpdyFramerVisitorInterface* visitor_;
  SpdyFramerDebugVisitorInterface* debug_visitor_;

  std::string display_protocol_;

  // The SPDY version to be spoken/understood by this framer. We support only
  // integer versions here, as major version numbers indicate framer-layer
  // incompatibility and minor version numbers indicate application-layer
  // incompatibility.
  const int spdy_version_;

  // Tracks if we've ever gotten far enough in framing to see a control frame of
  // type SYN_STREAM or SYN_REPLY.
  //
  // If we ever get something which looks like a data frame before we've had a
  // SYN, we explicitly check to see if it looks like we got an HTTP response to
  // a SPDY request.  This boolean lets us do that.
  bool syn_frame_processed_;

  // If we ever get a data frame before a SYN frame, we check to see if it
  // starts with HTTP.  If it does, we likely have an HTTP response.   This
  // isn't guaranteed though: we could have gotten a settings frame and then
  // corrupt data that just looks like HTTP, but deterministic checking requires
  // a lot more state.
  bool probable_http_response_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_FRAMER_H_
