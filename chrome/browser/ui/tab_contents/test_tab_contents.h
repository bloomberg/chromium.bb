// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

class TabContents;

namespace content {
class WebContents;
}

class TabContentsTestHarness : public ChromeRenderViewHostTestHarness {
 public:
  TabContentsTestHarness();
  virtual ~TabContentsTestHarness();

  virtual content::WebContents* web_contents() OVERRIDE;
  TabContents* tab_contents();

  virtual void SetContents(content::WebContents* contents) OVERRIDE;

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  scoped_ptr<TabContents> tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsTestHarness);
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
