// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NET_CURVECP_RECEIVED_BLOCK_LIST_H_
#define NET_CURVECP_RECEIVED_BLOCK_LIST_H_
#pragma once

#include <deque>

#include "base/memory/ref_counted.h"

namespace net {

class DrainableIOBuffer;
class IOBuffer;
class IOBufferWithSize;

// A ReceivedBlockList manages everything needed to track a set of of incoming
// blocks.  It can coalesce continuous blocks and locate holes in the received
// data.
// Note:  this list does not expect to receive overlapping blocks such as:
//        Block 1: bytes 10-20
//        Block 2: bytes 15-25
class ReceivedBlockList {
 public:
  ReceivedBlockList();
  ~ReceivedBlockList();

  // Returns the total number of bytes received in order.
  int bytes_received() const { return bytes_received_; }

  // Returns the number of currently available bytes to read.
  int bytes_available() const;

  // Adds an incoming block into the list of received blocks.
  void AddBlock(int64 position, IOBuffer* data, int length);

  // Reads data from the buffer starting at |current_position()|.
  // |buffer| is the buffer to fill.
  // |size| is the maximum number of bytes to be filled into |buffer|.
  // Returns the number of bytes read.
  int ReadBytes(IOBuffer* buffer, int size);

  // Gets the current position of the received data in the stream.
  // This is for testing only.
  int current_position() const { return current_position_; }

  // Debugging: Writes the entire blocklist to the log.
  void LogBlockList() const;

 private:
  typedef struct {
    int64 position;                         // Position of this block.
    scoped_refptr<DrainableIOBuffer> data;  // The data.
  } Block;
  typedef std::deque<Block> BlockList;

  // Recomputes |bytes_received_|.
  void ComputeBytesReceived();

  int64 current_position_;
  int64 bytes_received_;
  BlockList list_;

  DISALLOW_COPY_AND_ASSIGN(ReceivedBlockList);
};

}  // namespace net

#endif  // NET_CURVECP_RECEIVED_BLOCK_LIST_H_
