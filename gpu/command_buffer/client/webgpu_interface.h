// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_

#include <dawn/dawn.h>

#include "gpu/command_buffer/client/interface_base.h"

extern "C" typedef struct _ClientBuffer* ClientBuffer;
extern "C" typedef struct _GLColorSpace* GLColorSpace;

namespace gpu {
namespace webgpu {

class WebGPUInterface : public InterfaceBase {
 public:
  WebGPUInterface() {}
  virtual ~WebGPUInterface() {}

  virtual const DawnProcTable& GetProcs() const = 0;
  virtual void FlushCommands() = 0;
  virtual DawnDevice GetDefaultDevice() = 0;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_interface_autogen.h"
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_
