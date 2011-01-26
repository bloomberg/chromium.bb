// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the common parts of command buffer formats.

#ifndef GPU_COMMAND_BUFFER_COMMON_CMD_BUFFER_COMMON_H_
#define GPU_COMMAND_BUFFER_COMMON_CMD_BUFFER_COMMON_H_

#include "../common/types.h"
#include "../common/bitfield_helpers.h"
#include "../common/logging.h"

namespace gpu {

namespace cmd {
  enum ArgFlags {
    kFixed = 0x0,
    kAtLeastN = 0x1
  };
}  // namespace cmd

// Computes the number of command buffer entries needed for a certain size. In
// other words it rounds up to a multiple of entries.
inline uint32 ComputeNumEntries(size_t size_in_bytes) {
  return static_cast<uint32>(
      (size_in_bytes + sizeof(uint32) - 1) / sizeof(uint32));  // NOLINT
}

// Rounds up to a multiple of entries in bytes.
inline size_t RoundSizeToMultipleOfEntries(size_t size_in_bytes) {
  return ComputeNumEntries(size_in_bytes) * sizeof(uint32);  // NOLINT
}

// Struct that defines the command header in the command buffer.
struct CommandHeader {
  Uint32 size:21;
  Uint32 command:11;

  static const int32 kMaxSize = (1 << 21) - 1;

  void Init(uint32 _command, int32 _size) {
    GPU_DCHECK_LE(_size, kMaxSize);
    command = _command;
    size = _size;
  }

  // Sets the header based on the passed in command. Can not be used for
  // variable sized commands like immediate commands or Noop.
  template <typename T>
  void SetCmd() {
    COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
    Init(T::kCmdId, ComputeNumEntries(sizeof(T)));  // NOLINT
  }

  // Sets the header by a size in bytes of the immediate data after the command.
  template <typename T>
  void SetCmdBySize(uint32 size_of_data_in_bytes) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    Init(T::kCmdId,
         ComputeNumEntries(sizeof(T) + size_of_data_in_bytes));  // NOLINT
  }

  // Sets the header by a size in bytes.
  template <typename T>
  void SetCmdByTotalSize(uint32 size_in_bytes) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    GPU_DCHECK_GE(size_in_bytes, sizeof(T));  // NOLINT
    Init(T::kCmdId, ComputeNumEntries(size_in_bytes));
  }
};

COMPILE_ASSERT(sizeof(CommandHeader) == 4, Sizeof_CommandHeader_is_not_4);

// Union that defines possible command buffer entries.
union CommandBufferEntry {
  CommandHeader value_header;
  Uint32 value_uint32;
  Int32 value_int32;
  float value_float;
};

const size_t kCommandBufferEntrySize = 4;

COMPILE_ASSERT(sizeof(CommandBufferEntry) == kCommandBufferEntrySize,
               Sizeof_CommandBufferEntry_is_not_4);

// Make sure the compiler does not add extra padding to any of the command
// structures.
#pragma pack(push, 1)

// Gets the address of memory just after a structure in a typesafe way. This is
// used for IMMEDIATE commands to get the address of the place to put the data.
// Immediate command put their data direclty in the command buffer.
// Parameters:
//   cmd: Address of command.
template <typename T>
void* ImmediateDataAddress(T* cmd) {
  COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
  return reinterpret_cast<char*>(cmd) + sizeof(*cmd);
}

// Gets the address of the place to put the next command in a typesafe way.
// This can only be used for fixed sized commands.
template <typename T>
// Parameters:
//   cmd: Address of command.
void* NextCmdAddress(void* cmd) {
  COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
  return reinterpret_cast<char*>(cmd) + sizeof(T);
}

// Gets the address of the place to put the next command in a typesafe way.
// This can only be used for variable sized command like IMMEDIATE commands.
// Parameters:
//   cmd: Address of command.
//   size_of_data_in_bytes: Size of the data for the command.
template <typename T>
void* NextImmediateCmdAddress(void* cmd, uint32 size_of_data_in_bytes) {
  COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
  return reinterpret_cast<char*>(cmd) + sizeof(T) +   // NOLINT
      RoundSizeToMultipleOfEntries(size_of_data_in_bytes);
}

// Gets the address of the place to put the next command in a typesafe way.
// This can only be used for variable sized command like IMMEDIATE commands.
// Parameters:
//   cmd: Address of command.
//   size_of_cmd_in_bytes: Size of the cmd and data.
template <typename T>
void* NextImmediateCmdAddressTotalSize(void* cmd, uint32 total_size_in_bytes) {
  COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
  GPU_DCHECK_GE(total_size_in_bytes, sizeof(T));  // NOLINT
  return reinterpret_cast<char*>(cmd) +
      RoundSizeToMultipleOfEntries(total_size_in_bytes);
}

namespace cmd {

// This macro is used to safely and convienently expand the list of commnad
// buffer commands in to various lists and never have them get out of sync. To
// add a new command, add it this list, create the corresponding structure below
// and then add a function in gapi_decoder.cc called Handle_COMMAND_NAME where
// COMMAND_NAME is the name of your command structure.
//
// NOTE: THE ORDER OF THESE MUST NOT CHANGE (their id is derived by order)
#define COMMON_COMMAND_BUFFER_CMDS(OP) \
  OP(Noop)                          /*  0 */ \
  OP(SetToken)                      /*  1 */ \
  OP(Jump)                          /*  2 */ \
  OP(JumpRelative)                  /*  3 */ \
  OP(Call)                          /*  4 */ \
  OP(CallRelative)                  /*  5 */ \
  OP(Return)                        /*  6 */ \
  OP(SetBucketSize)                 /*  7 */ \
  OP(SetBucketData)                 /*  8 */ \
  OP(SetBucketDataImmediate)        /*  9 */ \
  OP(GetBucketSize)                 /* 10 */ \
  OP(GetBucketData)                 /* 11 */ \

// Common commands.
enum CommandId {
  #define COMMON_COMMAND_BUFFER_CMD_OP(name) k ## name,

  COMMON_COMMAND_BUFFER_CMDS(COMMON_COMMAND_BUFFER_CMD_OP)

  #undef COMMON_COMMAND_BUFFER_CMD_OP

  kNumCommands,
  kLastCommonId = 255  // reserve 256 spaces for common commands.
};

COMPILE_ASSERT(kNumCommands - 1 <= kLastCommonId, Too_many_common_commands);

const char* GetCommandName(CommandId id);

// A Noop command.
struct Noop {
  typedef Noop ValueType;
  static const CommandId kCmdId = kNoop;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;

  void SetHeader(uint32 skip_count) {
    GPU_DCHECK_GT(skip_count, 0u);
    header.Init(kCmdId, skip_count);
  }

  void Init(uint32 skip_count) {
    SetHeader(skip_count);
  }

  static void* Set(void* cmd, uint32 skip_count) {
    static_cast<ValueType*>(cmd)->Init(skip_count);
    return NextImmediateCmdAddress<ValueType>(
        cmd, skip_count * sizeof(CommandBufferEntry));  // NOLINT
  }

  CommandHeader header;
};

COMPILE_ASSERT(sizeof(Noop) == 4, Sizeof_Noop_is_not_4);
COMPILE_ASSERT(offsetof(Noop, header) == 0, Offsetof_Noop_header_not_0);

// The SetToken command puts a token in the command stream that you can
// use to check if that token has been passed in the command stream.
struct SetToken {
  typedef SetToken ValueType;
  static const CommandId kCmdId = kSetToken;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _token) {
    SetHeader();
    token = _token;
  }
  static void* Set(void* cmd, uint32 token) {
    static_cast<ValueType*>(cmd)->Init(token);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 token;
};

COMPILE_ASSERT(sizeof(SetToken) == 8, Sizeof_SetToken_is_not_8);
COMPILE_ASSERT(offsetof(SetToken, header) == 0,
               Offsetof_SetToken_header_not_0);
COMPILE_ASSERT(offsetof(SetToken, token) == 4,
               Offsetof_SetToken_token_not_4);

// The Jump command jumps to another place in the command buffer.
struct Jump {
  typedef Jump ValueType;
  static const CommandId kCmdId = kJump;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _offset) {
    SetHeader();
    offset = _offset;
  }
  static void* Set(
      void* cmd, uint32 _offset) {
    static_cast<ValueType*>(cmd)->Init(_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 offset;
};

COMPILE_ASSERT(sizeof(Jump) == 8, Sizeof_Jump_is_not_8);
COMPILE_ASSERT(offsetof(Jump, header) == 0,
               Offsetof_Jump_header_not_0);
COMPILE_ASSERT(offsetof(Jump, offset) == 4,
               Offsetof_Jump_offset_not_4);

// The JumpRelative command jumps to another place in the command buffer
// relative to the end of this command. In other words. JumpRelative with an
// offset of zero is effectively a noop.
struct JumpRelative {
  typedef JumpRelative ValueType;
  static const CommandId kCmdId = kJumpRelative;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(int32 _offset) {
    SetHeader();
    offset = _offset;
  }
  static void* Set(void* cmd, int32 _offset) {
    static_cast<ValueType*>(cmd)->Init(_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  int32 offset;
};

COMPILE_ASSERT(sizeof(JumpRelative) == 8, Sizeof_JumpRelative_is_not_8);
COMPILE_ASSERT(offsetof(JumpRelative, header) == 0,
               Offsetof_JumpRelative_header_not_0);
COMPILE_ASSERT(offsetof(JumpRelative, offset) == 4,
               Offsetof_JumpRelative_offset_4);

// The Call command jumps to a subroutine which can be returned from with the
// Return command.
struct Call {
  typedef Call ValueType;
  static const CommandId kCmdId = kCall;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _offset) {
    SetHeader();
    offset = _offset;
  }
  static void* Set(void* cmd, uint32 _offset) {
    static_cast<ValueType*>(cmd)->Init(_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 offset;
};

COMPILE_ASSERT(sizeof(Call) == 8, Sizeof_Call_is_not_8);
COMPILE_ASSERT(offsetof(Call, header) == 0,
               Offsetof_Call_header_not_0);
COMPILE_ASSERT(offsetof(Call, offset) == 4,
               Offsetof_Call_offset_not_4);

// The CallRelative command jumps to a subroutine using a relative offset. The
// offset is relative to the end of this command..
struct CallRelative {
  typedef CallRelative ValueType;
  static const CommandId kCmdId = kCallRelative;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(int32 _offset) {
    SetHeader();
    offset = _offset;
  }
  static void* Set(void* cmd, int32 _offset) {
    static_cast<ValueType*>(cmd)->Init(_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  int32 offset;
};

COMPILE_ASSERT(sizeof(CallRelative) == 8, Sizeof_CallRelative_is_not_8);
COMPILE_ASSERT(offsetof(CallRelative, header) == 0,
               Offsetof_CallRelative_header_not_0);
COMPILE_ASSERT(offsetof(CallRelative, offset) == 4,
               Offsetof_CallRelative_offset_4);

// Returns from a subroutine called by the Call or CallRelative commands.
struct Return {
  typedef Return ValueType;
  static const CommandId kCmdId = kReturn;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init() {
    SetHeader();
  }
  static void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
};

COMPILE_ASSERT(sizeof(Return) == 4, Sizeof_Return_is_not_4);
COMPILE_ASSERT(offsetof(Return, header) == 0,
               Offsetof_Return_header_not_0);

// Sets the size of a bucket for collecting data on the service side.
// This is a utility for gathering data on the service side so it can be used
// all at once when some service side API is called. It removes the need to add
// special commands just to support a particular API. For example, any API
// command that needs a string needs a way to send that string to the API over
// the command buffers. While you can require that the command buffer or
// transfer buffer be large enough to hold the largest string you can send,
// using this command removes that restriction by letting you send smaller
// pieces over and build up the data on the service side.
//
// You can clear a bucket on the service side and thereby free memory by sending
// a size of 0.
struct SetBucketSize {
  typedef SetBucketSize ValueType;
  static const CommandId kCmdId = kSetBucketSize;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _bucket_id, uint32 _size) {
    SetHeader();
    bucket_id = _bucket_id;
    size = _size;
  }
  static void* Set(void* cmd, uint32 _bucket_id, uint32 _size) {
    static_cast<ValueType*>(cmd)->Init(_bucket_id, _size);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 bucket_id;
  uint32 size;
};

COMPILE_ASSERT(sizeof(SetBucketSize) == 12, Sizeof_SetBucketSize_is_not_8);
COMPILE_ASSERT(offsetof(SetBucketSize, header) == 0,
               Offsetof_SetBucketSize_header_not_0);
COMPILE_ASSERT(offsetof(SetBucketSize, bucket_id) == 4,
               Offsetof_SetBucketSize_bucket_id_4);
COMPILE_ASSERT(offsetof(SetBucketSize, size) == 8,
               Offsetof_SetBucketSize_size_8);

// Sets the contents of a portion of a bucket on the service side from data in
// shared memory.
// See SetBucketSize.
struct SetBucketData {
  typedef SetBucketData ValueType;
  static const CommandId kCmdId = kSetBucketData;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _bucket_id,
            uint32 _offset,
            uint32 _size,
            uint32 _shared_memory_id,
            uint32 _shared_memory_offset) {
    SetHeader();
    bucket_id = _bucket_id;
    offset = _offset;
    size = _size;
    shared_memory_id = _shared_memory_id;
    shared_memory_offset = _shared_memory_offset;
  }
  static void* Set(void* cmd,
                   uint32 _bucket_id,
                   uint32 _offset,
                   uint32 _size,
                   uint32 _shared_memory_id,
                   uint32 _shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(
        _bucket_id,
        _offset,
        _size,
        _shared_memory_id,
        _shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 bucket_id;
  uint32 offset;
  uint32 size;
  uint32 shared_memory_id;
  uint32 shared_memory_offset;
};

COMPILE_ASSERT(sizeof(SetBucketData) == 24, Sizeof_SetBucketData_is_not_24);
COMPILE_ASSERT(offsetof(SetBucketData, header) == 0,
               Offsetof_SetBucketData_header_not_0);
COMPILE_ASSERT(offsetof(SetBucketData, bucket_id) == 4,
               Offsetof_SetBucketData_bucket_id_not_4);
COMPILE_ASSERT(offsetof(SetBucketData, offset) == 8,
               Offsetof_SetBucketData_offset_not_8);
COMPILE_ASSERT(offsetof(SetBucketData, size) == 12,
               Offsetof_SetBucketData_size_not_12);
COMPILE_ASSERT(offsetof(SetBucketData, shared_memory_id) == 16,
               Offsetof_SetBucketData_shared_memory_id_not_16);
COMPILE_ASSERT(offsetof(SetBucketData, shared_memory_offset) == 20,
               Offsetof_SetBucketData_shared_memory_offset_not_20);

// Sets the contents of a portion of a bucket on the service side from data in
// the command buffer.
// See SetBucketSize.
struct SetBucketDataImmediate {
  typedef SetBucketDataImmediate ValueType;
  static const CommandId kCmdId = kSetBucketDataImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;

  void SetHeader(uint32 size) {
    header.SetCmdBySize<ValueType>(size);
  }

  void Init(uint32 _bucket_id,
            uint32 _offset,
            uint32 _size) {
    SetHeader(_size);
    bucket_id = _bucket_id;
    offset = _offset;
    size = _size;
  }
  static void* Set(void* cmd,
                   uint32 _bucket_id,
                   uint32 _offset,
                   uint32 _size) {
    static_cast<ValueType*>(cmd)->Init(
        _bucket_id,
        _offset,
        _size);
    return NextImmediateCmdAddress<ValueType>(cmd, _size);
  }

  CommandHeader header;
  uint32 bucket_id;
  uint32 offset;
  uint32 size;
};

COMPILE_ASSERT(sizeof(SetBucketDataImmediate) == 16,
               Sizeof_SetBucketDataImmediate_is_not_24);
COMPILE_ASSERT(offsetof(SetBucketDataImmediate, header) == 0,
               Offsetof_SetBucketDataImmediate_header_not_0);
COMPILE_ASSERT(offsetof(SetBucketDataImmediate, bucket_id) == 4,
               Offsetof_SetBucketDataImmediate_bucket_id_not_4);
COMPILE_ASSERT(offsetof(SetBucketDataImmediate, offset) == 8,
               Offsetof_SetBucketDataImmediate_offset_not_8);
COMPILE_ASSERT(offsetof(SetBucketDataImmediate, size) == 12,
               Offsetof_SetBucketDataImmediate_size_not_12);

// Gets the size of a bucket the service has available. Sending a variable size
// result back to the client, for example any API that returns a string, is
// problematic since the largest thing you can send back is the size of your
// shared memory. This command along with GetBucketData implements a way to get
// a result a piece at a time to help solve that problem in a generic way.
struct GetBucketSize {
  typedef GetBucketSize ValueType;
  static const CommandId kCmdId = kGetBucketSize;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  typedef uint32 Result;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _bucket_id,
            uint32 _shared_memory_id,
            uint32 _shared_memory_offset) {
    SetHeader();
    bucket_id = _bucket_id;
    shared_memory_id = _shared_memory_id;
    shared_memory_offset = _shared_memory_offset;
  }
  static void* Set(void* cmd,
                   uint32 _bucket_id,
                   uint32 _shared_memory_id,
                   uint32 _shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(
        _bucket_id,
        _shared_memory_id,
        _shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 bucket_id;
  uint32 shared_memory_id;
  uint32 shared_memory_offset;
};

COMPILE_ASSERT(sizeof(GetBucketSize) == 16, Sizeof_GetBucketSize_is_not_16);
COMPILE_ASSERT(offsetof(GetBucketSize, header) == 0,
               Offsetof_GetBucketSize_header_not_0);
COMPILE_ASSERT(offsetof(GetBucketSize, bucket_id) == 4,
               Offsetof_GetBucketSize_bucket_id_not_4);
COMPILE_ASSERT(offsetof(GetBucketSize, shared_memory_id) == 8,
               Offsetof_GetBucketSize_shared_memory_id_not_8);
COMPILE_ASSERT(offsetof(GetBucketSize, shared_memory_offset) == 12,
               Offsetof_GetBucketSize_shared_memory_offset_not_12);

// Gets a piece of a result the service as available.
// See GetBucketSize.
struct GetBucketData {
  typedef GetBucketData ValueType;
  static const CommandId kCmdId = kGetBucketData;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(uint32 _bucket_id,
            uint32 _offset,
            uint32 _size,
            uint32 _shared_memory_id,
            uint32 _shared_memory_offset) {
    SetHeader();
    bucket_id = _bucket_id;
    offset = _offset;
    size = _size;
    shared_memory_id = _shared_memory_id;
    shared_memory_offset = _shared_memory_offset;
  }
  static void* Set(void* cmd,
                   uint32 _bucket_id,
                   uint32 _offset,
                   uint32 _size,
                   uint32 _shared_memory_id,
                   uint32 _shared_memory_offset) {
    static_cast<ValueType*>(cmd)->Init(
        _bucket_id,
        _offset,
        _size,
        _shared_memory_id,
        _shared_memory_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  CommandHeader header;
  uint32 bucket_id;
  uint32 offset;
  uint32 size;
  uint32 shared_memory_id;
  uint32 shared_memory_offset;
};

COMPILE_ASSERT(sizeof(GetBucketData) == 24, Sizeof_GetBucketData_is_not_20);
COMPILE_ASSERT(offsetof(GetBucketData, header) == 0,
               Offsetof_GetBucketData_header_not_0);
COMPILE_ASSERT(offsetof(GetBucketData, bucket_id) == 4,
               Offsetof_GetBucketData_bucket_id_not_4);
COMPILE_ASSERT(offsetof(GetBucketData, offset) == 8,
               Offsetof_GetBucketData_offset_not_8);
COMPILE_ASSERT(offsetof(GetBucketData, size) == 12,
               Offsetof_GetBucketData_size_not_12);
COMPILE_ASSERT(offsetof(GetBucketData, shared_memory_id) == 16,
               Offsetof_GetBucketData_shared_memory_id_not_16);
COMPILE_ASSERT(offsetof(GetBucketData, shared_memory_offset) == 20,
               Offsetof_GetBucketData_shared_memory_offset_not_20);

}  // namespace cmd

#pragma pack(pop)

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_CMD_BUFFER_COMMON_H_

