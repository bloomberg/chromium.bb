// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_VIEWS_H_

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
class Point;
}

namespace views {
class Widget;
}

class RenderViewContextMenuViews : public RenderViewContextMenu {
 public:
  virtual ~RenderViewContextMenuViews();

  // Factory function to create an instance.
  static RenderViewContextMenuViews* Create(
      content::RenderFrameHost* render_frame_host,
      const content::ContextMenuParams& params);

  void RunMenuAt(views::Widget* parent,
                 const gfx::Point& point,
                 ui::MenuSourceType type);

  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 protected:
  RenderViewContextMenuViews(content::RenderFrameHost* render_frame_host,
                             const content::ContextMenuParams& params);

  // RenderViewContextMenu implementation.
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

 private:
  virtual void AppendPlatformEditableItems() OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;

  // Model for the BiDi input submenu.
  ui::SimpleMenuModel bidi_submenu_model_;
  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_VIEWS_H_
