// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/menu_model.h"

using ui::MenuModel;

TestRenderViewContextMenu::TestRenderViewContextMenu(
    content::RenderFrameHost* render_frame_host,
    content::ContextMenuParams params)
    : RenderViewContextMenu(render_frame_host, params) {}

TestRenderViewContextMenu::~TestRenderViewContextMenu() {}

// static
TestRenderViewContextMenu* TestRenderViewContextMenu::Create(
    content::WebContents* web_contents,
    const GURL& page_url,
    const GURL& link_url,
    const GURL& frame_url) {
  content::ContextMenuParams params;
  params.page_url = page_url;
  params.link_url = link_url;
  params.frame_url = frame_url;
  TestRenderViewContextMenu* menu =
      new TestRenderViewContextMenu(web_contents->GetMainFrame(), params);
  menu->Init();
  return menu;
}

void TestRenderViewContextMenu::PlatformInit() {}

void TestRenderViewContextMenu::PlatformCancel() {}

bool TestRenderViewContextMenu::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  // None of our commands have accelerators, so always return false.
  return false;
}

bool TestRenderViewContextMenu::IsItemPresent(int command_id) {
  return menu_model_.GetIndexOfCommandId(command_id) != -1;
}

bool TestRenderViewContextMenu::GetMenuModelAndItemIndex(
    int command_id,
    MenuModel** found_model,
    int* found_index) {
  std::vector<MenuModel*> models_to_search;
  models_to_search.push_back(&menu_model_);

  while (!models_to_search.empty()) {
    MenuModel* model = models_to_search.back();
    models_to_search.pop_back();
    for (int i = 0; i < model->GetItemCount(); i++) {
      if (model->GetCommandIdAt(i) == command_id) {
        *found_model = model;
        *found_index = i;
        return true;
      } else if (model->GetTypeAt(i) == MenuModel::TYPE_SUBMENU) {
        models_to_search.push_back(model->GetSubmenuModelAt(i));
      }
    }
  }

  return false;
}
