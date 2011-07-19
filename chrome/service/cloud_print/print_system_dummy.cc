// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is dummy implementation for all configurations where print system
// for cloud print is not available.
#if !defined(CP_PRINT_SYSTEM_AVAILABLE)

#include "chrome/service/cloud_print/print_system.h"

#include "base/logging.h"

namespace cloud_print {

std::string PrintSystem::GenerateProxyId() {
  NOTREACHED();
  return std::string();
}

scoped_refptr<PrintSystem> PrintSystem::CreateInstance(
    const base::DictionaryValue* print_system_settings) {
  NOTREACHED();
  return NULL;
}
}  // namespace cloud_print

#endif  // CP_PRINT_SYSTEM_AVAILABLE
