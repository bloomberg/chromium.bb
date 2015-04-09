// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/public/test/content_browser_test.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;
class Shell;

class SitePerProcessBrowserTest : public ContentBrowserTest {
 public:
  SitePerProcessBrowserTest();

  // Returns an alphabetically-sorted, newline-delimited list of the site
  // instance URLs in which RenderFrameProxyHosts of |node| currently exist.
  // TODO(nick): Make this a full-fledged tree walk.
  static std::string DumpProxyHostSiteInstances(FrameTreeNode* node);

 protected:
  // Start at a data URL so each extra navigation creates a navigation entry.
  // (The first navigation will silently be classified as AUTO_SUBFRAME.)
  // TODO(creis): This won't be necessary when we can wait for LOAD_STOP.
  void StartFrameAtDataURL();

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
};

}  // namespace content
