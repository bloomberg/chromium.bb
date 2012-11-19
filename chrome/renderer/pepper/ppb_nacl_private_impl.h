// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PPB_NACL_PRIVATE_IMPL_H_
#define CHROME_RENDERER_PEPPER_PPB_NACL_PRIVATE_IMPL_H_

#include "build/build_config.h"

#ifndef DISABLE_NACL
#include "ppapi/c/private/ppb_nacl_private.h"

class PPB_NaCl_Private_Impl {
 public:
  static const PPB_NaCl_Private* GetInterface();
};

#endif  // DISABLE_NACL

#endif  // CHROME_RENDERER_PEPPER_PPB_NACL_PRIVATE_IMPL_H_
