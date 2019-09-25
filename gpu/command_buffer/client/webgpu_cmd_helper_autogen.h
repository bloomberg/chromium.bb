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

void AssociateMailboxImmediate(GLuint device_id,
                               GLuint device_generation,
                               GLuint id,
                               GLuint generation,
                               GLuint usage,
                               const GLbyte* mailbox) {
  const uint32_t size = webgpu::cmds::AssociateMailboxImmediate::ComputeSize();
  webgpu::cmds::AssociateMailboxImmediate* c =
      GetImmediateCmdSpaceTotalSize<webgpu::cmds::AssociateMailboxImmediate>(
          size);
  if (c) {
    c->Init(device_id, device_generation, id, generation, usage, mailbox);
  }
}

void DissociateMailbox(GLuint texture_id, GLuint texture_generation) {
  webgpu::cmds::DissociateMailbox* c =
      GetCmdSpace<webgpu::cmds::DissociateMailbox>();
  if (c) {
    c->Init(texture_id, texture_generation);
  }
}

void RequestAdapter(uint32_t power_preference) {
  webgpu::cmds::RequestAdapter* c = GetCmdSpace<webgpu::cmds::RequestAdapter>();
  if (c) {
    c->Init(power_preference);
  }
}

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_CMD_HELPER_AUTOGEN_H_
