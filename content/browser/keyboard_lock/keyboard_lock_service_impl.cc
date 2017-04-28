// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/keyboard_lock/keyboard_lock_service_impl.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

KeyboardLockServiceImpl::KeyboardLockServiceImpl() = default;

KeyboardLockServiceImpl::~KeyboardLockServiceImpl() = default;

// static
void KeyboardLockServiceImpl::CreateMojoService(
    blink::mojom::KeyboardLockServiceRequest request) {
  mojo::MakeStrongBinding(
        base::MakeUnique<KeyboardLockServiceImpl>(),
        std::move(request));
}

void KeyboardLockServiceImpl::RequestKeyLock(
    const std::vector<std::string>& key_codes,
    const RequestKeyLockCallback& callback) {
  // TODO(zijiehe): Implementation required.
  callback.Run(true, std::string());
}

void KeyboardLockServiceImpl::CancelKeyLock() {
  // TODO(zijiehe): Implementation required.
}

}  // namespace content
