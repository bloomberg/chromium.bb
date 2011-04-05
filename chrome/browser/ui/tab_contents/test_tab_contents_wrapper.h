// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_TEST_TAB_CONTENTS_WRAPPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_TEST_TAB_CONTENTS_WRAPPER_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/browser/renderer_host/test_render_view_host.h"

class TabContentsWrapper;

class TabContentsWrapperTestHarness : public RenderViewHostTestHarness {
 public:
  TabContentsWrapperTestHarness();
  virtual ~TabContentsWrapperTestHarness();

  virtual TestTabContents* contents() OVERRIDE;
  TabContentsWrapper* contents_wrapper();

  void SetContents(TestTabContents* contents);

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  scoped_ptr<TabContentsWrapper> contents_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsWrapperTestHarness);
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_TEST_TAB_CONTENTS_WRAPPER_H_
