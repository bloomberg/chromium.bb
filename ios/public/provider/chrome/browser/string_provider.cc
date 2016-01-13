// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/string_provider.h"

namespace ios {

StringProvider::StringProvider() {}

StringProvider::~StringProvider() {}

std::string StringProvider::GetOmniboxCopyUrlString() {
  return std::string();
}

base::string16 StringProvider::GetProductName() {
  return base::string16();
}

}  // namespace ios
