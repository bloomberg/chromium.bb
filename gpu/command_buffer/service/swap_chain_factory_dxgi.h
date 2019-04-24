// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SWAP_CHAIN_FACTORY_DXGI_H_
#define GPU_COMMAND_BUFFER_SERVICE_SWAP_CHAIN_FACTORY_DXGI_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
class SharedImageBacking;
struct Mailbox;

class GPU_GLES2_EXPORT SwapChainFactoryDXGI {
 public:
  SwapChainFactoryDXGI();
  ~SwapChainFactoryDXGI();

  struct SwapChainBackings {
    SwapChainBackings(std::unique_ptr<SharedImageBacking> front_buffer,
                      std::unique_ptr<SharedImageBacking> back_buffer);
    ~SwapChainBackings();
    SwapChainBackings(SwapChainBackings&&);
    SwapChainBackings& operator=(SwapChainBackings&&);

    std::unique_ptr<SharedImageBacking> front_buffer;
    std::unique_ptr<SharedImageBacking> back_buffer;

   private:
    DISALLOW_COPY_AND_ASSIGN(SwapChainBackings);
  };

  // Creates IDXGI Swap Chain and exposes front and back buffers as Shared Image
  // mailboxes.
  SwapChainBackings CreateSwapChain(const Mailbox& front_buffer_mailbox,
                                    const Mailbox& back_buffer_mailbox,
                                    viz::ResourceFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    uint32_t usage);

 private:
  DISALLOW_COPY_AND_ASSIGN(SwapChainFactoryDXGI);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SWAP_CHAIN_FACTORY_DXGI_H_