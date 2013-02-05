/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "kernel_proxy_mock.h"
#include "nacl_io/kernel_intercept.h"

KernelProxyMock::KernelProxyMock() {
}

KernelProxyMock::~KernelProxyMock() {
  // Uninitialize the kernel proxy so wrapped functions passthrough to their
  // unwrapped versions.
  ki_uninit();
}
