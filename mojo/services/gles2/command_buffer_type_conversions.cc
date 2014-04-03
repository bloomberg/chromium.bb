// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/gles2/command_buffer_type_conversions.h"

#include "mojo/services/gles2/command_buffer.mojom.h"

namespace mojo {

CommandBufferState
TypeConverter<CommandBufferState, gpu::CommandBuffer::State>::ConvertFrom(
    const gpu::CommandBuffer::State& input,
    Buffer* buffer) {
  CommandBufferState::Builder result(buffer);
  result.set_num_entries(input.num_entries);
  result.set_get_offset(input.get_offset);
  result.set_put_offset(input.put_offset);
  result.set_token(input.token);
  result.set_error(input.error);
  result.set_context_lost_reason(input.context_lost_reason);
  result.set_generation(input.generation);
  return result.Finish();
}

gpu::CommandBuffer::State
TypeConverter<CommandBufferState, gpu::CommandBuffer::State>::ConvertTo(
    const CommandBufferState& input) {
  gpu::CommandBuffer::State state;
  state.num_entries = input.num_entries();
  state.get_offset = input.get_offset();
  state.put_offset = input.put_offset();
  state.token = input.token();
  state.error = static_cast<gpu::error::Error>(input.error());
  state.context_lost_reason =
      static_cast<gpu::error::ContextLostReason>(input.context_lost_reason());
  state.generation = input.generation();
  return state;
}

}  // namespace mojo
