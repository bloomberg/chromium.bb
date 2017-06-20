// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_stream_parser.h"

#include <algorithm>

#include "base/containers/stack_container.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/profiling/memlog_stream.h"
#include "chrome/profiling/address.h"
#include "chrome/profiling/backtrace.h"
#include "chrome/profiling/profiling_globals.h"

namespace profiling {

namespace {

using AddressVector = base::StackVector<Address, 128>;

}  // namespace

MemlogStreamParser::Block::Block(std::unique_ptr<char[]> d, size_t s)
    : data(std::move(d)), size(s) {}

MemlogStreamParser::MemlogStreamParser(MemlogReceiver* receiver)
    : receiver_(receiver) {}

MemlogStreamParser::~MemlogStreamParser() {}

void MemlogStreamParser::OnStreamData(std::unique_ptr<char[]> data, size_t sz) {
  blocks_.emplace_back(std::move(data), sz);

  if (!received_header_) {
    received_header_ = true;
    ReadStatus status = ParseHeader();
    if (status != READ_OK)
      return;  // TODO(brettw) signal error.
  }

  while (true) {
    uint32_t msg_type;
    if (!PeekBytes(sizeof(msg_type), &msg_type))
      return;

    ReadStatus status;
    switch (msg_type) {
      case kAllocPacketType:
        status = ParseAlloc();
        break;
      case kFreePacketType:
        status = ParseFree();
        break;
      default:
        return;  // TODO(brettw) signal error.
    }
    if (status != READ_OK)
      return;  // TODO(brettw) signal error.
  }
}

void MemlogStreamParser::OnStreamComplete() {
  receiver_->OnComplete();
}

bool MemlogStreamParser::AreBytesAvailable(size_t count) const {
  size_t used = 0;
  size_t current_block_offset = block_zero_offset_;
  for (auto it = blocks_.begin(); it != blocks_.end() && used < count; ++it) {
    used += it->size - current_block_offset;
    current_block_offset = 0;
  }
  return used >= count;
}

bool MemlogStreamParser::PeekBytes(size_t count, void* dest) const {
  char* dest_char = static_cast<char*>(dest);
  size_t used = 0;

  size_t current_block_offset = block_zero_offset_;
  for (const auto& block : blocks_) {
    size_t in_current_block = block.size - current_block_offset;
    size_t to_copy = std::min(count - used, in_current_block);

    memcpy(&dest_char[used], &block.data[current_block_offset], to_copy);
    used += to_copy;

    // All subsequent blocks start reading at offset 0.
    current_block_offset = 0;
  }
  return used == count;
}

bool MemlogStreamParser::ReadBytes(size_t count, void* dest) {
  if (!PeekBytes(count, dest))
    return false;
  ConsumeBytes(count);
  return true;
}

void MemlogStreamParser::ConsumeBytes(size_t count) {
  DCHECK(AreBytesAvailable(count));
  while (count > 0) {
    size_t bytes_left_in_block = blocks_.front().size - block_zero_offset_;
    if (bytes_left_in_block > count) {
      // Still data left in this block;
      block_zero_offset_ += count;
      return;
    }

    // Current block is consumed.
    blocks_.pop_front();
    block_zero_offset_ = 0;
    count -= bytes_left_in_block;
  }
}

MemlogStreamParser::ReadStatus MemlogStreamParser::ParseHeader() {
  StreamHeader header;
  if (!ReadBytes(sizeof(StreamHeader), &header))
    return READ_NO_DATA;

  if (header.signature != kStreamSignature)
    return READ_ERROR;

  receiver_->OnHeader(header);
  return READ_OK;
}

MemlogStreamParser::ReadStatus MemlogStreamParser::ParseAlloc() {
  // Read the packet. Can't commit the read until the stack is read and
  // that has to be done below.
  AllocPacket alloc_packet;
  if (!PeekBytes(sizeof(AllocPacket), &alloc_packet))
    return READ_NO_DATA;

  std::vector<Address> stack;
  stack.resize(alloc_packet.stack_len);
  size_t stack_byte_size = sizeof(Address) * alloc_packet.stack_len;

  if (!AreBytesAvailable(sizeof(AllocPacket) + stack_byte_size))
    return READ_NO_DATA;

  // Everything will fit, mark packet consumed, read stack.
  ConsumeBytes(sizeof(AllocPacket));
  if (!stack.empty())
    ReadBytes(stack_byte_size, stack.data());

  receiver_->OnAlloc(alloc_packet, std::move(stack));
  return READ_OK;
}

MemlogStreamParser::ReadStatus MemlogStreamParser::ParseFree() {
  FreePacket free_packet;
  if (!ReadBytes(sizeof(FreePacket), &free_packet))
    return READ_NO_DATA;

  receiver_->OnFree(free_packet);
  return READ_OK;
}

}  // namespace profiling
