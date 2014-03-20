// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_PPB_NACL_PRIVATE_IMPL_H_
#define COMPONENTS_NACL_RENDERER_PPB_NACL_PRIVATE_IMPL_H_

#include "build/build_config.h"

#include "ppapi/c/private/ppb_nacl_private.h"

namespace nacl {

const PPB_NaCl_Private* GetNaClPrivateInterface();

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_PPB_NACL_PRIVATE_IMPL_H_
