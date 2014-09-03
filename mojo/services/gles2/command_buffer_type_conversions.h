// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_
#define MOJO_SERVICES_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_

#include "gpu/command_buffer/common/command_buffer.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/services/gles2/command_buffer.mojom.h"

namespace mojo {

class CommandBufferState;

template <>
struct TypeConverter<CommandBufferStatePtr, gpu::CommandBuffer::State> {
  static CommandBufferStatePtr Convert(const gpu::CommandBuffer::State& input);
};

template <>
struct TypeConverter<gpu::CommandBuffer::State, CommandBufferStatePtr> {
  static gpu::CommandBuffer::State Convert(const CommandBufferStatePtr& input);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_
