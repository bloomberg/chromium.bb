// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/public/cpp/args.h"

namespace mojo {

// Create native viewport in headless mode.
const char kUseHeadlessConfig[] = "use-headless-config";
// Force gl to be initialized in test mode.
const char kUseTestConfig[] = "use-test-config";

}  // namespace mojo
