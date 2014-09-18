// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/gcapi_test_registry_overrider.h"

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

GCAPITestRegistryOverrider::GCAPITestRegistryOverrider() {
  // Override keys - this is undone during destruction.
  override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
  override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE);
}

GCAPITestRegistryOverrider::~GCAPITestRegistryOverrider() {
}
