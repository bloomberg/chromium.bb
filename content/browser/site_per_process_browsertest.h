// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test.h"
#include "url/gurl.h"

namespace content {

class Shell;

class SitePerProcessBrowserTest : public ContentBrowserTest {
 public:
  SitePerProcessBrowserTest();

 protected:
  // Start at a data URL so each extra navigation creates a navigation entry.
  // (The first navigation will silently be classified as AUTO_SUBFRAME.)
  // TODO(creis): This won't be necessary when we can wait for LOAD_STOP.
  void StartFrameAtDataURL();

  bool NavigateIframeToURL(Shell* window,
                           const GURL& url,
                           std::string iframe_id);

  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;
};

}  // namespace content
