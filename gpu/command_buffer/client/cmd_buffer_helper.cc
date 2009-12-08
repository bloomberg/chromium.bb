/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the implementation of the command buffer helper class.

#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/common/command_buffer.h"

namespace command_buffer {

using command_buffer::CommandBuffer;

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
  if (!ring_buffer_)
    return false;

  // Map the ring buffer into this process.
  if (!ring_buffer_->Map(ring_buffer_->max_size()))
    return false;

  entries_ = static_cast<CommandBufferEntry*>(ring_buffer_->memory());
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

}  // namespace command_buffer
