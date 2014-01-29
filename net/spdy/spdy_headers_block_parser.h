// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_HEADERS_BLOCK_PARSER_H_
#define NET_SPDY_SPDY_HEADERS_BLOCK_PARSER_H_

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net {

// A handler class for SPDY headers.
class SpdyHeadersHandlerInterface {
 public:
  virtual ~SpdyHeadersHandlerInterface() {}

  // A callback method which notifies when the parser starts handling a new
  // SPDY headers block, this method also notifies on the number of headers in
  // the block.
  virtual void OnHeaderBlock(uint32_t num_of_headers) = 0;

  // A callback method which notifies when the parser finishes handling a SPDY
  // headers block.
  virtual void OnHeaderBlockEnd() = 0;

  // A callback method which notifies on a SPDY header key value pair.
  virtual void OnKeyValuePair(const base::StringPiece& key,
                              const base::StringPiece& value) = 0;
};

// Helper class which simplifies reading contiguously
// from a disjoint buffer prefix & suffix.
class NET_EXPORT_PRIVATE SpdyHeadersBlockParserReader {
 public:
  // StringPieces are treated as raw buffers.
  SpdyHeadersBlockParserReader(base::StringPiece prefix,
                               base::StringPiece suffix);
  // Number of bytes available to be read.
  size_t Available();

  // Reads |count| bytes, copying into |*out|. Returns true on success,
  // false if not enough bytes were available.
  bool ReadN(size_t count, char* out);

  // Returns a new string contiguously holding
  // the remaining unread prefix and suffix.
  std::vector<char> Remainder();

 private:
  base::StringPiece prefix_;
  base::StringPiece suffix_;
  bool in_suffix_;
  size_t offset_;
};

// This class handles SPDY headers block bytes and parses out key-value pairs
// as they arrive. This class is not thread-safe, and assumes that all headers
// block bytes are processed in a single thread.
class NET_EXPORT_PRIVATE SpdyHeadersBlockParser {
 public:
  // Costructor. The handler's OnKeyValuePair will be called for every key
  // value pair that we parsed from the headers block.
  explicit SpdyHeadersBlockParser(SpdyHeadersHandlerInterface* handler);

  virtual ~SpdyHeadersBlockParser();

  // Handles headers block data as it arrives.
  void HandleControlFrameHeadersData(const char* headers_data, size_t len);

 private:
  // The state of the parser.
  enum ParserState {
    READING_HEADER_BLOCK_LEN,
    READING_KEY_LEN,
    READING_KEY,
    READING_VALUE_LEN,
    READING_VALUE
  };

  ParserState state_;

  typedef SpdyHeadersBlockParserReader Reader;

  // Parses an unsigned 32 bit integer from the reader. This method assumes that
  // the integer is in network order and converts to host order. Returns true on
  // success and false on failure (if there were not enough bytes available).
  static bool ParseUInt32(Reader* reader, uint32_t* parsed_value);

  // Resets the state of the parser to prepare it for a headers block of a
  // new frame.
  void Reset();

  // Number of key-value pairs until we complete handling the current
  // headers block.
  uint32_t remaining_key_value_pairs_for_frame_;

  // The length of the next field in the headers block (either a key or
  // a value).
  uint32_t next_field_len_;

  // The length of the key of the current header.
  uint32_t key_len_;

  // Holds unprocessed bytes of the headers block.
  std::vector<char> headers_block_prefix_;

  // Handles key-value pairs as we parse them.
  SpdyHeadersHandlerInterface* handler_;

  // Points to the current key.
  scoped_ptr<char[]> current_key;

  // Points to the current value.
  scoped_ptr<char[]> current_value;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_HEADERS_BLOCK_PARSER_H_
