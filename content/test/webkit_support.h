// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_WEBKIT_SUPPORT_H_
#define CONTENT_TEST_WEBKIT_SUPPORT_H_

// This package provides functions used by webkit_unit_tests.
namespace content {

// Initializes or terminates a test environment for unit tests.
void SetUpTestEnvironmentForUnitTests();
void TearDownTestEnvironment();

}  // namespace content

#endif  // CONTENT_TEST_WEBKIT_SUPPORT_H_
