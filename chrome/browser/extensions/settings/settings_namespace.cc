// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_namespace.h"

#include "base/logging.h"

namespace extensions {

namespace settings_namespace {

namespace {
const char* kLocalNamespace = "local";
const char* kSyncNamespace  = "sync";
}  // namespace

std::string ToString(Namespace settings_namespace) {
  switch (settings_namespace) {
    case LOCAL: return kLocalNamespace;
    case SYNC:  return kSyncNamespace;
    case INVALID:
    default: NOTREACHED();
  }
  return std::string();
}

Namespace FromString(const std::string& namespace_string) {
  if (namespace_string == kLocalNamespace)
    return LOCAL;
  else if (namespace_string == kSyncNamespace)
    return SYNC;
  return INVALID;
}

}  // namespace settings_namespace

}  // namespace extensions
