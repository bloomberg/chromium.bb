// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_GTK_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_GTK_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "ui/gfx/point.h"

namespace content {
class RenderWidgetHostView;
struct ContextMenuParams;
}

class RenderViewContextMenuGtk : public RenderViewContextMenu,
                                 public MenuGtk::Delegate {
 public:
  RenderViewContextMenuGtk(content::WebContents* web_contents,
                           const content::ContextMenuParams& params,
                           content::RenderWidgetHostView* view);

  virtual ~RenderViewContextMenuGtk();

  // Show the menu at the given location.
  void Popup(const gfx::Point& point);

  // Menu::Delegate implementation ---------------------------------------------
  virtual bool AlwaysShowIconForCmd(int command_id) const OVERRIDE;

  // RenderViewContextMenuDelegate implementation ------------------------------
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const string16& title) OVERRIDE;

 protected:
  // RenderViewContextMenu implementation --------------------------------------
  virtual void PlatformInit() OVERRIDE;
  virtual void PlatformCancel() OVERRIDE;
  // TODO(port): implement.
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

 private:
  scoped_ptr<MenuGtk> menu_gtk_;
  uint32_t triggering_event_time_;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_GTK_H_
