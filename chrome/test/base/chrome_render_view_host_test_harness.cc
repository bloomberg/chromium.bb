// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_host_test_harness.h"

#include "chrome/test/base/testing_profile.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#endif

using content::RenderViewHostTester;
using content::RenderViewHostTestHarness;

ChromeRenderViewHostTestHarness::ChromeRenderViewHostTestHarness()
    : RenderViewHostTestHarness() {
}

ChromeRenderViewHostTestHarness::~ChromeRenderViewHostTestHarness() {
}

TestingProfile* ChromeRenderViewHostTestHarness::profile() {
  return static_cast<TestingProfile*>(browser_context_.get());
}

RenderViewHostTester* ChromeRenderViewHostTestHarness::rvh_tester() {
  return RenderViewHostTester::For(rvh());
}

void ChromeRenderViewHostTestHarness::SetUp() {
  if (!browser_context_.get())
    browser_context_.reset(new TestingProfile());
  RenderViewHostTestHarness::SetUp();
}

void ChromeRenderViewHostTestHarness::TearDown() {
  RenderViewHostTestHarness::TearDown();
#if defined(USE_ASH)
  ash::Shell::DeleteInstance();
#endif
#if defined(USE_AURA)
  aura::Env::DeleteInstance();
#endif
}
