// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_
#define CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/browser/renderer_host/test_render_view_host.h"

class TestingProfile;

// Wrapper around RenderViewHostTestHarness that uses a TestingProfile as
// browser context instead of a TestBrowserContext.
class ChromeRenderViewHostTestHarness : public RenderViewHostTestHarness {
 public:
  ChromeRenderViewHostTestHarness();
  virtual ~ChromeRenderViewHostTestHarness();

  TestingProfile* profile();

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
};

#endif  // CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_
