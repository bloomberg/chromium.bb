// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_PER_PROCESS_BROWSERTEST_H_
#define CONTENT_BROWSER_SITE_PER_PROCESS_BROWSERTEST_H_

#include <string>

#include "content/public/test/content_browser_test.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;
class Shell;

class SitePerProcessBrowserTest : public ContentBrowserTest {
 public:
  SitePerProcessBrowserTest();

 protected:
  std::string DepictFrameTree(FrameTreeNode* node);

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 private:
  FrameTreeVisualizer visualizer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SITE_PER_PROCESS_BROWSERTEST_H_
