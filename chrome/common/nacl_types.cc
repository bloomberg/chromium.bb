// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/nacl_types.h"

namespace nacl {

NaClStartParams::NaClStartParams()
    : validation_cache_enabled(false),
      enable_exception_handling(false),
      enable_debug_stub(false),
      enable_ipc_proxy(false),
      uses_irt(false),
      enable_dyncode_syscalls(false) {
}

NaClStartParams::~NaClStartParams() {
}

NaClLaunchParams::NaClLaunchParams()
    : render_view_id(0),
      permission_bits(0),
      uses_irt(false),
      enable_dyncode_syscalls(false),
      enable_exception_handling(false)  {
}

NaClLaunchParams::NaClLaunchParams(const std::string& manifest_url,
                                   int render_view_id,
                                   uint32 permission_bits,
                                   bool uses_irt,
                                   bool enable_dyncode_syscalls,
                                   bool enable_exception_handling)
    : manifest_url(manifest_url),
      render_view_id(render_view_id),
      permission_bits(permission_bits),
      uses_irt(uses_irt),
      enable_dyncode_syscalls(enable_dyncode_syscalls),
      enable_exception_handling(enable_exception_handling)  {
}

NaClLaunchParams::NaClLaunchParams(const NaClLaunchParams& l) {
  manifest_url = l.manifest_url;
  render_view_id = l.render_view_id;
  permission_bits = l.permission_bits;
  uses_irt = l.uses_irt;
  enable_dyncode_syscalls = l.enable_dyncode_syscalls;
  enable_exception_handling = l.enable_exception_handling;
}

NaClLaunchParams::~NaClLaunchParams() {
}

}  // namespace nacl
