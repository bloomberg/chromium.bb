// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function main(mojo) {
  mojo.gtest.expectTrue(mojo.core, "mojo.core");
  mojo.gtest.expectFalse(mojo.foo, "mojo.foo");
}
