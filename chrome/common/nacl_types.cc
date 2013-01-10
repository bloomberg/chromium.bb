// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/nacl_types.h"

namespace nacl {

NaClStartParams::NaClStartParams()
    : validation_cache_enabled(false),
      enable_exception_handling(false) {
}

NaClStartParams::~NaClStartParams() {
}

NaClLaunchParams::NaClLaunchParams()
    : render_view_id(0),
      permission_bits(0) {
}

NaClLaunchParams::NaClLaunchParams(const std::string& manifest_url_,
                                   int render_view_id_,
                                   uint32 permission_bits_)
    : manifest_url(manifest_url_),
      render_view_id(render_view_id_),
      permission_bits(permission_bits_) {
}

NaClLaunchParams::NaClLaunchParams(const NaClLaunchParams& l) {
  manifest_url = l.manifest_url;
  render_view_id = l.render_view_id;
  permission_bits = l.permission_bits;
}

NaClLaunchParams::~NaClLaunchParams() {
}

}  // namespace nacl
