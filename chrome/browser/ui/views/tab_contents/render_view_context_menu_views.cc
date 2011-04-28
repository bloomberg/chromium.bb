// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/render_view_context_menu_views.h"

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "views/accelerator.h"
#include "views/controls/menu/menu_2.h"

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuViews, public:

RenderViewContextMenuViews::RenderViewContextMenuViews(
    TabContents* tab_contents,
    const ContextMenuParams& params)
    : RenderViewContextMenu(tab_contents, params) {
}

RenderViewContextMenuViews::~RenderViewContextMenuViews() {
}

void RenderViewContextMenuViews::RunMenuAt(int x, int y) {
  menu_->RunContextMenuAt(gfx::Point(x, y));
}

#if defined(OS_WIN)
void RenderViewContextMenuViews::SetExternal() {
  external_ = true;
}
#endif

void RenderViewContextMenuViews::UpdateMenuItemStates() {
  menu_->UpdateStates();
}

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuViews, protected:

void RenderViewContextMenuViews::PlatformInit() {
  menu_.reset(new views::Menu2(&menu_model_));

#if defined(OS_WIN)
  if (external_) {
    // The external tab container needs to be notified by command
    // and not by index. So we are turning off the MNS_NOTIFYBYPOS
    // style.
    HMENU menu = GetMenuHandle();
    DCHECK(menu != NULL);

    MENUINFO mi = {0};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_STYLE | MIM_MENUDATA;
    mi.dwMenuData = reinterpret_cast<ULONG_PTR>(this);
    SetMenuInfo(menu, &mi);
  }
#endif
}

bool RenderViewContextMenuViews::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accel) {
  // There are no formally defined accelerators we can query so we assume
  // that Ctrl+C, Ctrl+V, Ctrl+X, Ctrl-A, etc do what they normally do.
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_UNDO:
      *accel = views::Accelerator(ui::VKEY_Z, false, true, false);
      return true;

    case IDC_CONTENT_CONTEXT_REDO:
      // TODO(jcampan): should it be Ctrl-Y?
      *accel = views::Accelerator(ui::VKEY_Z, true, true, false);
      return true;

    case IDC_CONTENT_CONTEXT_CUT:
      *accel = views::Accelerator(ui::VKEY_X, false, true, false);
      return true;

    case IDC_CONTENT_CONTEXT_COPY:
      *accel = views::Accelerator(ui::VKEY_C, false, true, false);
      return true;

    case IDC_CONTENT_CONTEXT_PASTE:
      *accel = views::Accelerator(ui::VKEY_V, false, true, false);
      return true;

    case IDC_CONTENT_CONTEXT_SELECTALL:
      *accel = views::Accelerator(ui::VKEY_A, false, true, false);
      return true;

    default:
      return false;
  }
}
