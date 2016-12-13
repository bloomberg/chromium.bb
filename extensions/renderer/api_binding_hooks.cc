// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_binding_hooks.h"

namespace extensions {

APIBindingHooks::APIBindingHooks() {}
APIBindingHooks::~APIBindingHooks() {}

void APIBindingHooks::RegisterHandleRequest(const std::string& method_name,
                                            const HandleRequestHook& hook) {
  DCHECK(!hooks_used_) << "Hooks must be registered before the first use!";
  request_hooks_[method_name] = hook;
}

APIBindingHooks::HandleRequestHook APIBindingHooks::GetHandleRequest(
    const std::string& method_name) {
  hooks_used_ = true;
  auto iter = request_hooks_.find(method_name);
  if (iter != request_hooks_.end())
    return iter->second;

  return HandleRequestHook();
}

}  // namespace extensions
