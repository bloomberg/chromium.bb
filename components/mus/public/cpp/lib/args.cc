// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/args.h"

namespace mus {

// Initializes X11 in threaded mode, and sets the |override_redirect| flag when
// creating X11 windows.
const char kUseX11TestConfig[] = "use-x11-test-config";

}  // namespace mus
