/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A collection of debugging related interfaces.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_UTILITY_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_UTILITY_H_

#include <stdint.h>

#include "components/nacl/renderer/ppb_nacl_private.h"

#define SRPC_PLUGIN_DEBUG 1

namespace plugin {

const PPB_NaCl_Private* GetNaClInterface();
void SetNaClInterface(const PPB_NaCl_Private* nacl_interface);

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_UTILITY_H_
