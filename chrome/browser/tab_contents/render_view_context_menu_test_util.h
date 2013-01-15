// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_

#include "base/basictypes.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"

class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  TestRenderViewContextMenu(content::WebContents* web_contents,
                            content::ContextMenuParams params);
  virtual ~TestRenderViewContextMenu();

  virtual void PlatformInit() OVERRIDE;
  virtual void PlatformCancel() OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

  // Returns true if command specified by |command_id| is present
  // in the menu.
  // List of command ids can be found in chrome/app/chrome_command_ids.h.
  bool IsItemPresent(int command_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRenderViewContextMenu);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_
