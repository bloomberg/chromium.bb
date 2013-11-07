// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_GCAPI_GCAPI_TEST_REGISTRY_OVERRIDER_H_
#define CHROME_INSTALLER_GCAPI_GCAPI_TEST_REGISTRY_OVERRIDER_H_

#include "base/test/test_reg_util_win.h"

// Overrides the registry throughout its lifetime; used by GCAPI tests
// overriding the registry.
class GCAPITestRegistryOverrider {
 public:
  GCAPITestRegistryOverrider();
  ~GCAPITestRegistryOverrider();

 private:
  registry_util::RegistryOverrideManager override_manager_;

  DISALLOW_COPY_AND_ASSIGN(GCAPITestRegistryOverrider);
};

#endif  // CHROME_INSTALLER_GCAPI_GCAPI_TEST_REGISTRY_OVERRIDER_H_
