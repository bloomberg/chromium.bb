// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_AUTOGEN_H_

#define GL_SCANOUT_CHROMIUM 0x6000

struct DawnCommands {
  typedef DawnCommands ValueType;
  static const CommandId kCmdId = kDawnCommands;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _commands_shm_id,
            uint32_t _commands_shm_offset,
            uint32_t _size) {
    SetHeader();
    commands_shm_id = _commands_shm_id;
    commands_shm_offset = _commands_shm_offset;
    size = _size;
  }

  void* Set(void* cmd,
            uint32_t _commands_shm_id,
            uint32_t _commands_shm_offset,
            uint32_t _size) {
    static_cast<ValueType*>(cmd)->Init(_commands_shm_id, _commands_shm_offset,
                                       _size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t commands_shm_id;
  uint32_t commands_shm_offset;
  uint32_t size;
};

static_assert(sizeof(DawnCommands) == 16, "size of DawnCommands should be 16");
static_assert(offsetof(DawnCommands, header) == 0,
              "offset of DawnCommands header should be 0");
static_assert(offsetof(DawnCommands, commands_shm_id) == 4,
              "offset of DawnCommands commands_shm_id should be 4");
static_assert(offsetof(DawnCommands, commands_shm_offset) == 8,
              "offset of DawnCommands commands_shm_offset should be 8");
static_assert(offsetof(DawnCommands, size) == 12,
              "offset of DawnCommands size should be 12");

struct Dummy {
  typedef Dummy ValueType;
  static const CommandId kCmdId = kDummy;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(Dummy) == 4, "size of Dummy should be 4");
static_assert(offsetof(Dummy, header) == 0,
              "offset of Dummy header should be 0");

#endif  // GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_AUTOGEN_H_
