// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_CMD_HELPER_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_CMD_HELPER_AUTOGEN_H_

void DawnCommands(uint32_t commands_shm_id,
                  uint32_t commands_shm_offset,
                  uint32_t size) {
  webgpu::cmds::DawnCommands* c = GetCmdSpace<webgpu::cmds::DawnCommands>();
  if (c) {
    c->Init(commands_shm_id, commands_shm_offset, size);
  }
}

void Dummy() {
  webgpu::cmds::Dummy* c = GetCmdSpace<webgpu::cmds::Dummy>();
  if (c) {
    c->Init();
  }
}

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_CMD_HELPER_AUTOGEN_H_
