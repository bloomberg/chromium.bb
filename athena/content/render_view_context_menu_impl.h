// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_RENDER_VIEW_CONTEXT_MENU_IMPL_H_
#define ATHENA_CONTENT_RENDER_VIEW_CONTEXT_MENU_IMPL_H_

#include "components/renderer_context_menu/render_view_context_menu_base.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace views {
class Widget;
}

namespace athena {

class RenderViewContextMenuImpl : public RenderViewContextMenuBase {
 public:
  RenderViewContextMenuImpl(content::RenderFrameHost* render_frame_host,
                            const content::ContextMenuParams& params);
  ~RenderViewContextMenuImpl() override;

  void RunMenuAt(views::Widget* parent,
                 const gfx::Point& point,
                 ui::MenuSourceType type);

  // RenderViewContextMenuBase:
  void Show() override;

 private:
  // RenderViewContextMenuBase:
  void InitMenu() override;
  void RecordShownItem(int id) override;
  void RecordUsedItem(int id) override;
#if defined(ENABLE_PLUGINS)
  void HandleAuthorizeAllPlugins() override;
#endif
  void NotifyMenuShown() override;
  void NotifyURLOpened(const GURL& url,
                       content::WebContents* new_contents) override;

  // ui::SimpleMenuModel:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  aura::Window* GetActiveNativeView();
  views::Widget* GetTopLevelWidget();

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuImpl);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_RENDER_VIEW_CONTEXT_MENU_IMPL_H_
