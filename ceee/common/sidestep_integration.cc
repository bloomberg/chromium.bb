// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Integrate SideStep into our project.

#include "base/logging.h"
#include "ceee/testing/sidestep/integration.h"

namespace sidestep {

void AssertImpl(bool assertion_is_true, const char* message) {
  DCHECK(assertion_is_true) << message;
}

void LogImpl(const char* message) {
  DLOG(INFO) << message;
}

}  // namespace sidestep
