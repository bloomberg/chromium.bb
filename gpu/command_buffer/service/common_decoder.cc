// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/precompile.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"

namespace gpu {

const void* CommonDecoder::Bucket::GetData(size_t offset, size_t size) const {
  if (OffsetSizeValid(offset, size)) {
    return data_.get() + offset;
  }
  return NULL;
}

void CommonDecoder::Bucket::SetSize(size_t size) {
  if (size != size_) {
    data_.reset(size ? new int8[size] : NULL);
    size_ = size;
    memset(data_.get(), 0, size);
  }
}

bool CommonDecoder::Bucket::SetData(
    const void* src, size_t offset, size_t size) {
  if (OffsetSizeValid(offset, size)) {
    memcpy(data_.get() + offset, src, size);
    return true;
  }
  return false;
}

void* CommonDecoder::GetAddressAndCheckSize(unsigned int shm_id,
                                            unsigned int offset,
                                            unsigned int size) {
  Buffer buffer = engine_->GetSharedMemoryBuffer(shm_id);
  if (!buffer.ptr)
    return NULL;
  unsigned int end = offset + size;
  if (end > buffer.size || end < offset) {
    return NULL;
  }
  return static_cast<int8*>(buffer.ptr) + offset;
}

const char* CommonDecoder::GetCommonCommandName(
    cmd::CommandId command_id) const {
  return cmd::GetCommandName(command_id);
}

CommonDecoder::Bucket* CommonDecoder::GetBucket(uint32 bucket_id) const {
  BucketMap::const_iterator iter(buckets_.find(bucket_id));
  return iter != buckets_.end() ? &(*iter->second) : NULL;
}

namespace {

// Returns the address of the first byte after a struct.
template <typename T>
const void* AddressAfterStruct(const T& pod) {
  return reinterpret_cast<const uint8*>(&pod) + sizeof(pod);
}

// Returns the address of the frst byte after the struct.
template <typename RETURN_TYPE, typename COMMAND_TYPE>
RETURN_TYPE GetImmediateDataAs(const COMMAND_TYPE& pod) {
  return static_cast<RETURN_TYPE>(const_cast<void*>(AddressAfterStruct(pod)));
}

// A struct to hold info about each command.
struct CommandInfo {
  int arg_flags;  // How to handle the arguments for this command
  int arg_count;  // How many arguments are expected for this command.
};

// A table of CommandInfo for all the commands.
const CommandInfo g_command_info[] = {
  #define COMMON_COMMAND_BUFFER_CMD_OP(name) {                           \
    cmd::name::kArgFlags,                                                \
    sizeof(cmd::name) / sizeof(CommandBufferEntry) - 1, },  /* NOLINT */ \

  COMMON_COMMAND_BUFFER_CMDS(COMMON_COMMAND_BUFFER_CMD_OP)

  #undef COMMON_COMMAND_BUFFER_CMD_OP
};

}  // anonymous namespace.

// Decode command with its arguments, and call the corresponding method.
// Note: args is a pointer to the command buffer. As such, it could be changed
// by a (malicious) client at any time, so if validation has to happen, it
// should operate on a copy of them.
parse_error::ParseError CommonDecoder::DoCommonCommand(
    unsigned int command,
    unsigned int arg_count,
    const void* cmd_data) {
  if (command < arraysize(g_command_info)) {
    const CommandInfo& info = g_command_info[command];
    unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
    if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
        (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
      uint32 immediate_data_size =
          (arg_count - info_arg_count) * sizeof(CommandBufferEntry);  // NOLINT
      switch (command) {
        #define COMMON_COMMAND_BUFFER_CMD_OP(name)                      \
          case cmd::name::kCmdId:                                       \
            return Handle ## name(                                      \
                immediate_data_size,                                    \
                *static_cast<const cmd::name*>(cmd_data));              \

        COMMON_COMMAND_BUFFER_CMDS(COMMON_COMMAND_BUFFER_CMD_OP)

        #undef COMMON_COMMAND_BUFFER_CMD_OP
      }
    } else {
      return parse_error::kParseInvalidArguments;
    }
  }
  return DoCommonCommand(command, arg_count, cmd_data);
  return parse_error::kParseUnknownCommand;
}

parse_error::ParseError CommonDecoder::HandleNoop(
    uint32 immediate_data_size,
    const cmd::Noop& args) {
  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleSetToken(
    uint32 immediate_data_size,
    const cmd::SetToken& args) {
  engine_->set_token(args.token);
  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleJump(
    uint32 immediate_data_size,
    const cmd::Jump& args) {
  DCHECK(false);  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleJumpRelative(
    uint32 immediate_data_size,
    const cmd::JumpRelative& args) {
  DCHECK(false);  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleCall(
    uint32 immediate_data_size,
    const cmd::Call& args) {
  DCHECK(false);  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleCallRelative(
    uint32 immediate_data_size,
    const cmd::CallRelative& args) {
  DCHECK(false);  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleReturn(
    uint32 immediate_data_size,
    const cmd::Return& args) {
  DCHECK(false);  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleSetBucketSize(
    uint32 immediate_data_size,
    const cmd::SetBucketSize& args) {
  uint32 bucket_id = args.bucket_id;
  uint32 size = args.size;

  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    bucket = new Bucket();
    buckets_[bucket_id] = linked_ptr<Bucket>(bucket);
  }

  bucket->SetSize(size);
  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleSetBucketData(
    uint32 immediate_data_size,
    const cmd::SetBucketData& args) {
  uint32 bucket_id = args.bucket_id;
  uint32 offset = args.offset;
  uint32 size = args.size;
  const void* data = GetSharedMemoryAs<const void*>(
      args.shared_memory_id, args.shared_memory_offset, size);
  if (!data) {
    return parse_error::kParseInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return parse_error::kParseInvalidArguments;
  }
  if (!bucket->SetData(data, offset, size)) {
    return parse_error::kParseInvalidArguments;
  }

  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleSetBucketDataImmediate(
    uint32 immediate_data_size,
    const cmd::SetBucketDataImmediate& args) {
  const void* data = GetImmediateDataAs<const void*>(args);
  uint32 bucket_id = args.bucket_id;
  uint32 offset = args.offset;
  uint32 size = args.size;
  if (size > immediate_data_size) {
    return parse_error::kParseInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return parse_error::kParseInvalidArguments;
  }
  if (!bucket->SetData(data, offset, size)) {
    return parse_error::kParseInvalidArguments;
  }
  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleGetBucketSize(
    uint32 immediate_data_size,
    const cmd::GetBucketSize& args) {
  uint32 bucket_id = args.bucket_id;
  uint32* data = GetSharedMemoryAs<uint32*>(
      args.shared_memory_id, args.shared_memory_offset, sizeof(*data));
  if (!data) {
    return parse_error::kParseInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return parse_error::kParseInvalidArguments;
  }
  *data = bucket->size();
  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleGetBucketData(
    uint32 immediate_data_size,
    const cmd::GetBucketData& args) {
  uint32 bucket_id = args.bucket_id;
  uint32 offset = args.offset;
  uint32 size = args.size;
  void* data = GetSharedMemoryAs<void*>(
      args.shared_memory_id, args.shared_memory_offset, size);
  if (!data) {
    return parse_error::kParseInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return parse_error::kParseInvalidArguments;
  }
  const void* src = bucket->GetData(offset, size);
  if (!src) {
      return parse_error::kParseInvalidArguments;
  }
  memcpy(data, src, size);
  return parse_error::kParseNoError;
}

}  // namespace gpu
