// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

int main(int argc, char** argv) {
  // This is used so we can link with base:test_support even for components that
  // don't have a main function.
  NOTREACHED();
  return 1;
}
