// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_STREAM_PARSER_H_
#define CHROME_PROFILING_MEMLOG_STREAM_PARSER_H_

#include <deque>

#include "base/macros.h"
#include "chrome/profiling/memlog_receiver.h"
#include "chrome/profiling/memlog_stream_receiver.h"

namespace profiling {

// Parses a memory stream. Refcounted via StreamReceiver.
class MemlogStreamParser : public MemlogStreamReceiver {
 public:
  // Receiver must outlive this class.
  explicit MemlogStreamParser(MemlogReceiver* receiver);
  ~MemlogStreamParser() override;

  // StreamReceiver implementation.
  void OnStreamData(std::unique_ptr<char[]> data, size_t sz) override;
  void OnStreamComplete() override;

 private:
  struct Block {
    Block(std::unique_ptr<char[]> d, size_t s);

    std::unique_ptr<char[]> data;
    size_t size;
  };

  enum ReadStatus {
    READ_OK,      // Read OK.
    READ_ERROR,   // Fatal error, don't send more data.
    READ_NO_DATA  // Not enough data, try again when we get more
  };

  // Returns true if the given number of bytes are available now.
  bool AreBytesAvailable(size_t count) const;

  // Returns false if not enough bytes are available. On failure, the dest
  // buffer will be in an undefined state (it may be written partially).
  bool PeekBytes(size_t count, void* dest) const;
  bool ReadBytes(size_t count, void* dest);
  void ConsumeBytes(size_t count);  // Bytes must be available.

  ReadStatus ParseHeader();
  ReadStatus ParseAlloc();
  ReadStatus ParseFree();

  MemlogReceiver* receiver_;  // Not owned by this class.

  std::deque<Block> blocks_;

  bool received_header_ = false;

  // Current offset into blocks_[0] of the next packet to process.
  size_t block_zero_offset_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MemlogStreamParser);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_STREAM_PARSER_H_
