// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"
#include "content/public/test/unittest_test_suite.h"

int main(int argc, char** argv) {
  return content::UnitTestTestSuite(
      new content::ContentTestSuite(argc, argv)).Run();
}
