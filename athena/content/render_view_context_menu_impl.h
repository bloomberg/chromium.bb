// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_RENDER_VIEW_CONTEXT_MENU_IMPL_H_
#define ATHENA_CONTENT_RENDER_VIEW_CONTEXT_MENU_IMPL_H_

#include "components/renderer_context_menu/render_view_context_menu_base.h"

namespace views {
class Widget;
}

namespace gfx {
class Point;
}

namespace athena {

class RenderViewContextMenuImpl : public RenderViewContextMenuBase {
 public:
  RenderViewContextMenuImpl(content::RenderFrameHost* render_frame_host,
                            const content::ContextMenuParams& params);
  virtual ~RenderViewContextMenuImpl();

  void RunMenuAt(views::Widget* parent,
                 const gfx::Point& point,
                 ui::MenuSourceType type);

 private:
  // RenderViewContextMenuBase:
  virtual void InitMenu() override;
  virtual void RecordShownItem(int id) override;
  virtual void RecordUsedItem(int id) override;
#if defined(ENABLE_PLUGINS)
  virtual void HandleAuthorizeAllPlugins() override;
#endif
  virtual void NotifyMenuShown() override;
  virtual void NotifyURLOpened(const GURL& url,
                               content::WebContents* new_contents) override;

  // ui::SimpleMenuModel:
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) override;
  virtual bool IsCommandIdChecked(int command_id) const override;
  virtual bool IsCommandIdEnabled(int command_id) const override;
  virtual void ExecuteCommand(int command_id, int event_flags) override;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuImpl);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_RENDER_VIEW_CONTEXT_MENU_IMPL_H_
