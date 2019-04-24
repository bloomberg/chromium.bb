// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/swap_chain_factory_dxgi.h"

#include <d3d11.h>
#include <dcomp.h>
#include <windows.h>
#include <wrl/client.h>

#include "gpu/command_buffer/service/shared_image_backing.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

// Implementation of SharedImageBacking that holds a reference to swap chain,
// wraps the swap chain buffer(front buffer/back buffer) into GLimage and
// creates a GL texture and stores it as gles2::Texture.
class SharedImageBackingDXGISwapChain : public SharedImageBacking {
 public:
  SharedImageBackingDXGISwapChain(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      size_t estimated_size,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain)
      : SharedImageBacking(mailbox,
                           format,
                           size,
                           color_space,
                           usage,
                           estimated_size,
                           false /* is_thread_safe */),
        swap_chain_(swap_chain) {
    DCHECK(swap_chain_);
    // TODO(ashithasantosh): Wrap swap chain buffer into GL image and create GL
    // texture.
  }

  ~SharedImageBackingDXGISwapChain() override { DCHECK(!swap_chain_); }

 private:
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingDXGISwapChain);
};

SwapChainFactoryDXGI::SwapChainFactoryDXGI() = default;

SwapChainFactoryDXGI::~SwapChainFactoryDXGI() = default;

SwapChainFactoryDXGI::SwapChainBackings::SwapChainBackings(
    std::unique_ptr<SharedImageBacking> front_buffer,
    std::unique_ptr<SharedImageBacking> back_buffer)
    : front_buffer(std::move(front_buffer)),
      back_buffer(std::move(back_buffer)) {}

SwapChainFactoryDXGI::SwapChainBackings::~SwapChainBackings() = default;

SwapChainFactoryDXGI::SwapChainBackings::SwapChainBackings(
    SwapChainFactoryDXGI::SwapChainBackings&&) = default;

SwapChainFactoryDXGI::SwapChainBackings&
SwapChainFactoryDXGI::SwapChainBackings::operator=(
    SwapChainFactoryDXGI::SwapChainBackings&&) = default;

SwapChainFactoryDXGI::SwapChainBackings SwapChainFactoryDXGI::CreateSwapChain(
    const Mailbox& front_buffer_mailbox,
    const Mailbox& back_buffer_mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  // TODO(ashithasantosh): Implement CreateSwapChain method.
  std::unique_ptr<SharedImageBackingDXGISwapChain> front_buffer;
  std::unique_ptr<SharedImageBackingDXGISwapChain> back_buffer;
  return {std::move(front_buffer), std::move(back_buffer)};
}

}  // namespace gpu
