// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of the command buffer helper class.

#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/common/command_buffer.h"

namespace gpu {

CommandBufferHelper::CommandBufferHelper(CommandBuffer* command_buffer)
    : command_buffer_(command_buffer),
      entries_(NULL),
      entry_count_(0),
      token_(0),
      last_token_read_(-1),
      get_(0),
      put_(0) {
}

bool CommandBufferHelper::Initialize() {
  ring_buffer_ = command_buffer_->GetRingBuffer();
  if (!ring_buffer_.ptr)
    return false;

  entries_ = static_cast<CommandBufferEntry*>(ring_buffer_.ptr);
  entry_count_ = command_buffer_->GetSize();
  get_ = command_buffer_->GetGetOffset();
  put_ = command_buffer_->GetPutOffset();
  last_token_read_ = command_buffer_->GetToken();

  return true;
}

CommandBufferHelper::~CommandBufferHelper() {
}

bool CommandBufferHelper::Flush() {
  get_ = command_buffer_->SyncOffsets(put_);
  return !command_buffer_->GetErrorStatus();
}

// Calls Flush() and then waits until the buffer is empty. Break early if the
// error is set.
bool CommandBufferHelper::Finish() {
  do {
    // Do not loop forever if the flush fails, meaning the command buffer reader
    // has shutdown).
    if (!Flush())
      return false;
  } while (put_ != get_);

  return true;
}

// Inserts a new token into the command stream. It uses an increasing value
// scheme so that we don't lose tokens (a token has passed if the current token
// value is higher than that token). Calls Finish() if the token value wraps,
// which will be rare.
int32 CommandBufferHelper::InsertToken() {
  // Increment token as 31-bit integer. Negative values are used to signal an
  // error.
  token_ = (token_ + 1) & 0x7FFFFFFF;
  CommandBufferEntry args;
  args.value_uint32 = token_;
  const uint32 kSetToken = 1;  // TODO(gman): add a common set of commands.
  AddCommand(kSetToken, 1, &args);
  if (token_ == 0) {
    // we wrapped
    Finish();
    last_token_read_ = command_buffer_->GetToken();
    DCHECK_EQ(token_, last_token_read_);
  }
  return token_;
}

// Waits until the current token value is greater or equal to the value passed
// in argument.
void CommandBufferHelper::WaitForToken(int32 token) {
  // Return immediately if corresponding InsertToken failed.
  if (token < 0)
    return;
  if (last_token_read_ >= token) return;  // fast path.
  if (token > token_) return;  // we wrapped
  Flush();
  last_token_read_ = command_buffer_->GetToken();
  while (last_token_read_ < token) {
    if (get_ == put_) {
      LOG(FATAL) << "Empty command buffer while waiting on a token.";
      return;
    }
    // Do not loop forever if the flush fails, meaning the command buffer reader
    // has shutdown.
    if (!Flush())
      return;
    last_token_read_ = command_buffer_->GetToken();
  }
}

// Waits for available entries, basically waiting until get >= put + count + 1.
// It actually waits for contiguous entries, so it may need to wrap the buffer
// around, adding noops. Thus this function may change the value of put_.
// The function will return early if an error occurs, in which case the
// available space may not be available.
void CommandBufferHelper::WaitForAvailableEntries(int32 count) {
  CHECK(count < entry_count_);
  if (put_ + count > entry_count_) {
    // There's not enough room between the current put and the end of the
    // buffer, so we need to wrap. We will add noops all the way to the end,
    // but we need to make sure get wraps first, actually that get is 1 or
    // more (since put will wrap to 0 after we add the noops).
    DCHECK_LE(1, put_);
    Flush();
    while (get_ > put_ || get_ == 0) {
      // Do not loop forever if the flush fails, meaning the command buffer
      // reader has shutdown.
      if (!Flush())
        return;
    }
    // Add the noops. By convention, a noop is a command 0 with no args.
    // TODO(apatrick): A noop can have a size. It would be better to add a
    //    single noop with a variable size. Watch out for size limit on
    //    individual commands.
    CommandHeader header;
    header.size = 1;
    header.command = 0;
    while (put_ < entry_count_) {
      entries_[put_++].value_header = header;
    }
    put_ = 0;
  }
  // If we have enough room, return immediatly.
  if (count <= AvailableEntries()) return;
  // Otherwise flush, and wait until we do have enough room.
  Flush();
  while (AvailableEntries() < count) {
    // Do not loop forever if the flush fails, meaning the command buffer reader
    // has shutdown.
    if (!Flush())
      return;
  }
}

CommandBufferEntry* CommandBufferHelper::GetSpace(uint32 entries) {
  WaitForAvailableEntries(entries);
  CommandBufferEntry* space = &entries_[put_];
  put_ += entries;
  return space;
}

parse_error::ParseError CommandBufferHelper::GetParseError() {
  int32 parse_error = command_buffer_->ResetParseError();
  return static_cast<parse_error::ParseError>(parse_error);
}

}  // namespace gpu
