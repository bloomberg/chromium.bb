// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_host_test_harness.h"

#include "chrome/test/base/testing_profile.h"

ChromeRenderViewHostTestHarness::ChromeRenderViewHostTestHarness()
    : RenderViewHostTestHarness() {
}

ChromeRenderViewHostTestHarness::~ChromeRenderViewHostTestHarness() {
}

TestingProfile* ChromeRenderViewHostTestHarness::profile() {
  return static_cast<TestingProfile*>(browser_context_.get());
}

void ChromeRenderViewHostTestHarness::SetUp() {
  if (!browser_context_.get())
    browser_context_.reset(new TestingProfile());
  RenderViewHostTestHarness::SetUp();
}

void ChromeRenderViewHostTestHarness::TearDown() {
  RenderViewHostTestHarness::TearDown();
}
