// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_GTK_H_
#define CHROME_BROWSER_UI_GTK_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_GTK_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "ui/gfx/point.h"

namespace content {
class RenderWidgetHostView;
struct ContextMenuParams;
}

class RenderViewContextMenuGtk : public RenderViewContextMenu,
                                 public MenuGtk::Delegate {
 public:
  RenderViewContextMenuGtk(content::RenderFrameHost* render_frame_host,
                           const content::ContextMenuParams& params,
                           content::RenderWidgetHostView* view);

  virtual ~RenderViewContextMenuGtk();

  // Show the menu at the given location.
  void Popup(const gfx::Point& point);

  // Menu::Delegate implementation ---------------------------------------------
  virtual bool AlwaysShowIconForCmd(int command_id) const OVERRIDE;

  // SimpleMenuModel::Delegate implementation.
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;

  // RenderViewContextMenuDelegate implementation ------------------------------
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const base::string16& title) OVERRIDE;

 protected:
  // RenderViewContextMenu implementation --------------------------------------
  virtual void PlatformInit() OVERRIDE;
  virtual void PlatformCancel() OVERRIDE;
  // TODO(port): implement.
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void AppendPlatformEditableItems() OVERRIDE;

 private:
  // Adds writing direction submenu.
  void AppendBidiSubMenu();

  // Model for the BiDi input submenu.
  ui::SimpleMenuModel bidi_submenu_model_;

  scoped_ptr<MenuGtk> menu_gtk_;
  uint32_t triggering_event_time_;
};

#endif  // CHROME_BROWSER_UI_GTK_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_GTK_H_
