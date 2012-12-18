/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "nacl_mounts/nacl_mounts.h"

#include <stdlib.h>
#include "nacl_mounts/kernel_intercept.h"

void nacl_mounts_init() {
  ki_init(NULL);
}

void nacl_mounts_init_ppapi(PP_Instance instance,
                            PPB_GetInterface get_interface) {
  ki_init_ppapi(NULL, instance, get_interface);
}
