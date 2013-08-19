// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_core_interface.h"

FakeCoreInterface::FakeCoreInterface() {}

void FakeCoreInterface::AddRefResource(PP_Resource handle) {
  return resource_manager_.AddRef(handle);
}

void FakeCoreInterface::ReleaseResource(PP_Resource handle) {
  return resource_manager_.Release(handle);
}
