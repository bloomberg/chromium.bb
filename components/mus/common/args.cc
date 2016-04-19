// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/common/args.h"
#include "components/mus/common/mus_common_export.h"

namespace mus {

// Initializes X11 in threaded mode, and sets the |override_redirect| flag when
// creating X11 windows. Also, exposes the WindowServerTest interface to clients
// when launched with this flag.
const char kUseTestConfig[] = "use-test-config";

}  // namespace mus
