// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu_test_util.h"

TestRenderViewContextMenu::TestRenderViewContextMenu(
    content::WebContents* web_contents,
    content::ContextMenuParams params)
    : RenderViewContextMenu(web_contents, params) {}

TestRenderViewContextMenu::~TestRenderViewContextMenu() {}

void TestRenderViewContextMenu::PlatformInit() {}

void TestRenderViewContextMenu::PlatformCancel() {}

bool TestRenderViewContextMenu::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

bool TestRenderViewContextMenu::IsItemPresent(int command_id) {
  return menu_model_.GetIndexOfCommandId(command_id) != -1;
}
