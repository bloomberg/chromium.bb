// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_GPU_PARAM_TRAITS_H_
#define GPU_IPC_GPU_PARAM_TRAITS_H_

#include "ipc/ipc_message_utils.h"
#include "gpu/command_buffer/common/command_buffer.h"

namespace IPC {

template <>
struct ParamTraits<gpu::CommandBuffer::State> {
  typedef gpu::CommandBuffer::State param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // GPU_IPC_GPU_PARAM_TRAITS_H_
