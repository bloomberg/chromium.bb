// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/keyboard_lock/keyboard_lock_service_impl.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

KeyboardLockServiceImpl::KeyboardLockServiceImpl(
    RenderFrameHost* render_frame_host)
    : web_contents_(WebContents::FromRenderFrameHost(render_frame_host)) {
  DCHECK(web_contents_);
}

KeyboardLockServiceImpl::~KeyboardLockServiceImpl() = default;

// static
void KeyboardLockServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    blink::mojom::KeyboardLockServiceRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<KeyboardLockServiceImpl>(render_frame_host),
      std::move(request));
}

void KeyboardLockServiceImpl::RequestKeyboardLock(
    const std::vector<std::string>& key_codes,
    RequestKeyboardLockCallback callback) {
  // TODO(zijiehe): Implementation required.
  std::move(callback).Run(blink::mojom::KeyboardLockRequestResult::SUCCESS);
}

void KeyboardLockServiceImpl::CancelKeyboardLock() {
  // TODO(zijiehe): Implementation required.
}

}  // namespace content
