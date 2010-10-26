// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_REGISTRY_H_
#define BASE_REGISTRY_H_
#pragma once

// TODO(brettw) remove this file when all callers are converted to using the
// new location & namespace.
#include "base/win/registry.h"

class RegKey : public base::win::RegKey {
 public:
  RegKey() {}
  RegKey(HKEY rootkey, const wchar_t* subkey, REGSAM access)
      : base::win::RegKey(rootkey, subkey, access) {}
  ~RegKey() { base::win::RegKey::~RegKey(); }
};

#endif  // BASE_REGISTRY_H_
