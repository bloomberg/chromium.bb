// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/webkit_unit_test_support.h"

#include "webkit/support/webkit_support.h"

namespace content {

void SetUpTestEnvironmentForWebKitUnitTests() {
  webkit_support::SetUpTestEnvironmentForUnitTests();
}

void TearDownEnvironmentForWebKitUnitTests() {
  webkit_support::TearDownTestEnvironment();
}

}
