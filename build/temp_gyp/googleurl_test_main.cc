// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a replacement for ../../googleurl/src/gurl_test_main.cc.  The
// unittest is being built for a Chromium tree, so ICU needs to be initialized
// how the Chromium tree expects which is different than what is in the
// GoogleURL version.

#include "base/i18n/icu_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
  base::mac::ScopedNSAutoreleasePool scoped_pool;

  testing::InitGoogleTest(&argc, argv);

  icu_util::Initialize();

  return RUN_ALL_TESTS();
}
