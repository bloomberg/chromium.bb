// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_
#define MOJO_SERVICES_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_

#include "gpu/command_buffer/common/command_buffer.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

class CommandBufferState;
class Buffer;

template <>
class TypeConverter<CommandBufferState, gpu::CommandBuffer::State> {
 public:
  static CommandBufferState ConvertFrom(const gpu::CommandBuffer::State& input,
                                        Buffer* buffer);
  static gpu::CommandBuffer::State ConvertTo(const CommandBufferState& input);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_
