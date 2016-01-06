// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/test/fake_string_provider.h"

namespace ios {

FakeStringProvider::FakeStringProvider() {}

FakeStringProvider::~FakeStringProvider() {}

std::string FakeStringProvider::GetOmniboxCopyUrlString() {
  return "Copy URL";
}

}  // namespace ios
