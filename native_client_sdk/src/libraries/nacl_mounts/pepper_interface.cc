/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "nacl_mounts/pepper_interface.h"

ScopedResource::ScopedResource(PepperInterface* ppapi, PP_Resource resource)
    : ppapi_(ppapi),
      resource_(resource) {
  ppapi_->AddRefResource(resource_);
}

ScopedResource::ScopedResource(PepperInterface* ppapi, PP_Resource resource,
                               NoAddRef)
    : ppapi_(ppapi),
      resource_(resource) {
}

ScopedResource::~ScopedResource() {
  ppapi_->ReleaseResource(resource_);
}
