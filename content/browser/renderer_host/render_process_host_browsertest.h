// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_BROWSERTEST_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_BROWSERTEST_H_
#pragma once

#include "chrome/test/in_process_browser_test.h"

class RenderProcessHostTest : public InProcessBrowserTest {
 public:
  RenderProcessHostTest();

  int RenderProcessHostCount();
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_BROWSERTEST_H_
