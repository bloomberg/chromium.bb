// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_WIN_H_

#include "chrome/browser/ui/views/tab_contents/render_view_context_menu_views.h"

namespace content {
class WebContents;
}

// Windows specific implementation of the render view context menu.
class RenderViewContextMenuWin : public RenderViewContextMenuViews {
 public:
  RenderViewContextMenuWin(content::RenderFrameHost* render_frame_host,
                           const content::ContextMenuParams& params);
  virtual ~RenderViewContextMenuWin();

  // SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdVisible(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_WIN_H_
