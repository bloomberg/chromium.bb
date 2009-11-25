// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the GLES2 command buffer commands.

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_H
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_H

// This is here because service side code must include the system's version of
// the GL headers where as client side code includes the Chrome version.
#ifdef GLES2_GPU_SERVICE
#include <GL/glew.h>
#if defined(OS_WIN)
#include <GL/wglew.h>
#endif
#else
#include <GLES2/gl2types.h>
#endif

#include "base/basictypes.h"
#include "gpu/command_buffer/common/types.h"
#include "gpu/command_buffer/common/bitfield_helpers.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/gles2_cmd_ids.h"

namespace command_buffer {
namespace gles2 {

#include "gpu/command_buffer/common/gles2_cmd_format_autogen.h"

// These are hand written commands.
// TODO(gman): Attempt to make these auto-generated.

struct GetAttribLocation {
  typedef GetAttribLocation ValueType;
  static const CommandId kCmdId = kGetAttribLocation;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  static uint32 ComputeSize() {
    return static_cast<uint32>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      GLuint _program, uint32 _name_shm_id, uint32 _name_shm_offset,
      uint32 _location_shm_id, uint32 _location_shm_offset,
      uint32 _data_size) {
    SetHeader();
    program = _program;
    name_shm_id = _name_shm_id;
    name_shm_offset = _name_shm_offset;
    location_shm_id = _location_shm_id;
    location_shm_offset = _location_shm_offset;
    data_size = _data_size;
  }

  void* Set(
      void* cmd, GLuint _program, uint32 _name_shm_id, uint32 _name_shm_offset,
      uint32 _location_shm_id, uint32 _location_shm_offset,
      uint32 _data_size) {
    static_cast<ValueType*>(
        cmd)->Init(
            _program, _name_shm_id, _name_shm_offset, _location_shm_id,
            _location_shm_offset, _data_size);
    return NextCmdAddress<ValueType>(cmd);
  }

  command_buffer::CommandHeader header;
  uint32 program;
  uint32 name_shm_id;
  uint32 name_shm_offset;
  uint32 location_shm_id;
  uint32 location_shm_offset;
  uint32 data_size;
};

COMPILE_ASSERT(sizeof(GetAttribLocation) == 28,
               Sizeof_GetAttribLocation_is_not_28);
COMPILE_ASSERT(offsetof(GetAttribLocation, header) == 0,
               OffsetOf_GetAttribLocation_header_not_0);
COMPILE_ASSERT(offsetof(GetAttribLocation, program) == 4,
               OffsetOf_GetAttribLocation_program_not_4);
COMPILE_ASSERT(offsetof(GetAttribLocation, name_shm_id) == 8,
               OffsetOf_GetAttribLocation_name_shm_id_not_8);
COMPILE_ASSERT(offsetof(GetAttribLocation, name_shm_offset) == 12,
               OffsetOf_GetAttribLocation_name_shm_offset_not_12);
COMPILE_ASSERT(offsetof(GetAttribLocation, location_shm_id) == 16,
               OffsetOf_GetAttribLocation_location_shm_id_not_16);
COMPILE_ASSERT(offsetof(GetAttribLocation, location_shm_offset) == 20,
               OffsetOf_GetAttribLocation_location_shm_offset_not_20);
COMPILE_ASSERT(offsetof(GetAttribLocation, data_size) == 24,
               OffsetOf_GetAttribLocation_data_size_not_24);

struct GetAttribLocationImmediate {
  typedef GetAttribLocationImmediate ValueType;
  static const CommandId kCmdId = kGetAttribLocationImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;

  static uint32 ComputeDataSize(const char* s) {
    return strlen(s);
  }

  static uint32 ComputeSize(const char* s) {
    return static_cast<uint32>(sizeof(ValueType) + ComputeDataSize(s));
  }

  void SetHeader(const char* s) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(s));
  }

  void Init(
      GLuint _program, const char* _name,
      uint32 _location_shm_id, uint32 _location_shm_offset) {
    SetHeader(_name);
    program = _program;
    location_shm_id = _location_shm_id;
    location_shm_offset = _location_shm_offset;
    data_size = ComputeDataSize(_name);
    memcpy(ImmediateDataAddress(this), _name, data_size);
  }

  void* Set(
      void* cmd, GLuint _program, const char* _name,
      uint32 _location_shm_id, uint32 _location_shm_offset) {
    uint32 total_size = ComputeSize(_name);
    static_cast<ValueType*>(
        cmd)->Init(_program, _name, _location_shm_id, _location_shm_offset);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, total_size);
  }

  command_buffer::CommandHeader header;
  uint32 program;
  uint32 location_shm_id;
  uint32 location_shm_offset;
  uint32 data_size;
};

COMPILE_ASSERT(sizeof(GetAttribLocationImmediate) == 20,
               Sizeof_GetAttribLocationImmediate_is_not_20);
COMPILE_ASSERT(offsetof(GetAttribLocationImmediate, header) == 0,
               OffsetOf_GetAttribLocationImmediate_header_not_0);
COMPILE_ASSERT(offsetof(GetAttribLocationImmediate, program) == 4,
               OffsetOf_GetAttribLocationImmediate_program_not_4);
COMPILE_ASSERT(offsetof(GetAttribLocationImmediate, location_shm_id) == 8,
               OffsetOf_GetAttribLocationImmediate_location_shm_id_not_8);
COMPILE_ASSERT(offsetof(GetAttribLocationImmediate, location_shm_offset) == 12,
               OffsetOf_GetAttribLocationImmediate_location_shm_offset_not_12);
COMPILE_ASSERT(offsetof(GetAttribLocationImmediate, data_size) == 16,
               OffsetOf_GetAttribLocationImmediate_data_size_not_16);

struct GetUniformLocation {
  typedef GetUniformLocation ValueType;
  static const CommandId kCmdId = kGetUniformLocation;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;

  static uint32 ComputeSize() {
    return static_cast<uint32>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() {
    header.SetCmd<ValueType>();
  }

  void Init(
      GLuint _program, uint32 _name_shm_id, uint32 _name_shm_offset,
      uint32 _location_shm_id, uint32 _location_shm_offset,
      uint32 _data_size) {
    SetHeader();
    program = _program;
    name_shm_id = _name_shm_id;
    name_shm_offset = _name_shm_offset;
    location_shm_id = _location_shm_id;
    location_shm_offset = _location_shm_offset;
    data_size = _data_size;
  }

  void* Set(
      void* cmd, GLuint _program, uint32 _name_shm_id, uint32 _name_shm_offset,
      uint32 _location_shm_id, uint32 _location_shm_offset,
      uint32 _data_size) {
    static_cast<ValueType*>(
        cmd)->Init(
            _program, _name_shm_id, _name_shm_offset, _location_shm_id,
            _location_shm_offset, _data_size);
    return NextCmdAddress<ValueType>(cmd);
  }

  command_buffer::CommandHeader header;
  uint32 program;
  uint32 name_shm_id;
  uint32 name_shm_offset;
  uint32 location_shm_id;
  uint32 location_shm_offset;
  uint32 data_size;
};

COMPILE_ASSERT(sizeof(GetUniformLocation) == 28,
               Sizeof_GetUniformLocation_is_not_28);
COMPILE_ASSERT(offsetof(GetUniformLocation, header) == 0,
               OffsetOf_GetUniformLocation_header_not_0);
COMPILE_ASSERT(offsetof(GetUniformLocation, program) == 4,
               OffsetOf_GetUniformLocation_program_not_4);
COMPILE_ASSERT(offsetof(GetUniformLocation, name_shm_id) == 8,
               OffsetOf_GetUniformLocation_name_shm_id_not_8);
COMPILE_ASSERT(offsetof(GetUniformLocation, name_shm_offset) == 12,
               OffsetOf_GetUniformLocation_name_shm_offset_not_12);
COMPILE_ASSERT(offsetof(GetUniformLocation, location_shm_id) == 16,
               OffsetOf_GetUniformLocation_location_shm_id_not_16);
COMPILE_ASSERT(offsetof(GetUniformLocation, location_shm_offset) == 20,
               OffsetOf_GetUniformLocation_location_shm_offset_not_20);
COMPILE_ASSERT(offsetof(GetUniformLocation, data_size) == 24,
               OffsetOf_GetUniformLocation_data_size_not_24);

struct GetUniformLocationImmediate {
  typedef GetUniformLocationImmediate ValueType;
  static const CommandId kCmdId = kGetUniformLocationImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;

  static uint32 ComputeDataSize(const char* s) {
    return strlen(s);
  }

  static uint32 ComputeSize(const char* s) {
    return static_cast<uint32>(sizeof(ValueType) + ComputeDataSize(s));
  }

  void SetHeader(const char* s) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(s));
  }

  void Init(
      GLuint _program, const char* _name,
      uint32 _location_shm_id, uint32 _location_shm_offset) {
    SetHeader(_name);
    program = _program;
    location_shm_id = _location_shm_id;
    location_shm_offset = _location_shm_offset;
    data_size = ComputeDataSize(_name);
    memcpy(ImmediateDataAddress(this), _name, data_size);
  }

  void* Set(
      void* cmd, GLuint _program, const char* _name,
      uint32 _location_shm_id, uint32 _location_shm_offset) {
    uint32 total_size = ComputeSize(_name);
    static_cast<ValueType*>(
        cmd)->Init(_program, _name, _location_shm_id, _location_shm_offset);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, total_size);
  }

  command_buffer::CommandHeader header;
  uint32 program;
  uint32 location_shm_id;
  uint32 location_shm_offset;
  uint32 data_size;
};

COMPILE_ASSERT(sizeof(GetUniformLocationImmediate) == 20,
               Sizeof_GetUniformLocationImmediate_is_not_20);
COMPILE_ASSERT(offsetof(GetUniformLocationImmediate, header) == 0,
               OffsetOf_GetUniformLocationImmediate_header_not_0);
COMPILE_ASSERT(offsetof(GetUniformLocationImmediate, program) == 4,
               OffsetOf_GetUniformLocationImmediate_program_not_4);
COMPILE_ASSERT(offsetof(GetUniformLocationImmediate, location_shm_id) == 8,
               OffsetOf_GetUniformLocationImmediate_location_shm_id_not_8);
COMPILE_ASSERT(
    offsetof(GetUniformLocationImmediate, location_shm_offset) == 12,
               OffsetOf_GetUniformLocationImmediate_location_shm_offset_not_12);
COMPILE_ASSERT(offsetof(GetUniformLocationImmediate, data_size) == 16,
               OffsetOf_GetUniformLocationImmediate_data_size_not_16);


}  // namespace gles2
}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_H

