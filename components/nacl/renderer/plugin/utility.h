/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_UTILITY_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_UTILITY_H_

#include "components/nacl/renderer/ppb_nacl_private.h"
#include "components/nacl/renderer/ppb_nacl_private_impl.h"

namespace plugin {

// TODO(mseaborn): Remove this and replace its uses with direct calls to
// the functions defined in ppb_nacl_private_impl.cc.
inline const PPB_NaCl_Private* GetNaClInterface() {
  return nacl::GetNaClPrivateInterface();
}

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_UTILITY_H_
