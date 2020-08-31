// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

namespace base {
struct Feature;
}
namespace chromecast {
std::vector<const base::Feature*> GetInternalFeatures() {
  return std::vector<const base::Feature*>();
}
}  // namespace chromecast
