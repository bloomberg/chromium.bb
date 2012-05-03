// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_VIEWS_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"

namespace gfx {
class Point;
}

namespace views {
class MenuItemView;
class MenuModelAdapter;
class MenuRunner;
class Widget;
}

class RenderViewContextMenuViews : public RenderViewContextMenu {
 public:
  RenderViewContextMenuViews(content::WebContents* tab_contents,
                             const content::ContextMenuParams& params);

  virtual ~RenderViewContextMenuViews();

  void RunMenuAt(views::Widget* parent, const gfx::Point& point);

#if defined(OS_WIN)
  // Set this menu to show for an external tab contents. This
  // only has an effect before Init() is called.
  void SetExternal();
#endif

  void UpdateMenuItemStates();

  // RenderViewContextMenuDelegate implementation.
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const string16& title) OVERRIDE;

 protected:
  // RenderViewContextMenu implementation.
  virtual void PlatformInit() OVERRIDE;
  virtual void PlatformCancel() OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

 private:
  // The context menu itself and its contents.
  scoped_ptr<views::MenuModelAdapter> menu_delegate_;
  views::MenuItemView* menu_;  // Owned by menu_runner_.
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_VIEWS_H_
