// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_

#include "base/basictypes.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "url/gurl.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/context_menu_matcher.h"
#endif

class Browser;

namespace content {
class WebContents;
}
namespace ui {
class MenuModel;
}

class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  TestRenderViewContextMenu(content::RenderFrameHost* render_frame_host,
                            content::ContextMenuParams params);
  ~TestRenderViewContextMenu() override;

  // Factory.
  // This is a lightweight method to create a test RenderViewContextMenu
  // instance.
  // Use the constructor if you want to create menu with fine-grained params.
  static TestRenderViewContextMenu* Create(content::WebContents* web_contents,
                                           const GURL& page_url,
                                           const GURL& link_url,
                                           const GURL& frame_url);

  // Implementation of pure virtuals in RenderViewContextMenu.
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;

  // Returns true if command specified by |command_id| is present
  // in the menu.
  // List of command ids can be found in chrome/app/chrome_command_ids.h.
  bool IsItemPresent(int command_id);

  // Searches for an menu item with |command_id|. If it's found, the return
  // value is true and the model and index where it appears in that model are
  // returned in |found_model| and |found_index|. Otherwise returns false.
  bool GetMenuModelAndItemIndex(int command_id,
                                ui::MenuModel** found_model,
                                int* found_index);

#if defined(ENABLE_EXTENSIONS)
  extensions::ContextMenuMatcher& extension_items() { return extension_items_; }
#endif

  void Show() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRenderViewContextMenu);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_
