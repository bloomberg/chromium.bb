// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "ppapi/nacl_irt/irt_manifest.h"

namespace nacl {
namespace nonsfi {

const nacl_irt_resource_open kIrtResourceOpen = {
  ppapi::IrtOpenResource,
};

}  // namespace nonsfi
}  // namespace nacl
