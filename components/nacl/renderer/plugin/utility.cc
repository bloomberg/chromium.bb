/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "components/nacl/renderer/plugin/utility.h"
#include "ppapi/cpp/module.h"

namespace plugin {

// We cache the NaCl interface pointer and ensure that its set early on, on the
// main thread. This allows GetNaClInterface() to be used from non-main threads.
static const PPB_NaCl_Private* g_nacl_interface = NULL;

const PPB_NaCl_Private* GetNaClInterface() {
  return g_nacl_interface;
}

void SetNaClInterface(const PPB_NaCl_Private* nacl_interface) {
  g_nacl_interface = nacl_interface;
}

}  // namespace plugin
