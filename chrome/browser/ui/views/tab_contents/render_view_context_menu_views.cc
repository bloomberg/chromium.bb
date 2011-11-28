// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/render_view_context_menu_views.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_views.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuViews, public:

RenderViewContextMenuViews::RenderViewContextMenuViews(
    TabContents* tab_contents,
    const ContextMenuParams& params)
    : RenderViewContextMenu(tab_contents, params),
      menu_(NULL) {
}

RenderViewContextMenuViews::~RenderViewContextMenuViews() {
}

void RenderViewContextMenuViews::RunMenuAt(int x, int y) {
  TabContentsViewViews* tab =
      static_cast<TabContentsViewViews*>(source_tab_contents_->view());
  views::Widget* parent = tab->GetTopLevelWidget();
  if (menu_runner_->RunMenuAt(parent, NULL,
          gfx::Rect(gfx::Point(x, y), gfx::Size()),
          views::MenuItemView::TOPLEFT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
}

#if defined(OS_WIN)
void RenderViewContextMenuViews::SetExternal() {
  external_ = true;
}
#endif

void RenderViewContextMenuViews::UpdateMenuItemStates() {
  menu_delegate_->BuildMenu(menu_);
}

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuViews, protected:

void RenderViewContextMenuViews::PlatformInit() {
  menu_delegate_.reset(new views::MenuModelAdapter(&menu_model_));
  menu_ = new views::MenuItemView(menu_delegate_.get());
  menu_runner_.reset(new views::MenuRunner(menu_));
  UpdateMenuItemStates();
}

bool RenderViewContextMenuViews::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accel) {
  // There are no formally defined accelerators we can query so we assume
  // that Ctrl+C, Ctrl+V, Ctrl+X, Ctrl-A, etc do what they normally do.
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_UNDO:
      *accel = ui::Accelerator(ui::VKEY_Z, false, true, false);
      return true;

    case IDC_CONTENT_CONTEXT_REDO:
      // TODO(jcampan): should it be Ctrl-Y?
      *accel = ui::Accelerator(ui::VKEY_Z, true, true, false);
      return true;

    case IDC_CONTENT_CONTEXT_CUT:
      *accel = ui::Accelerator(ui::VKEY_X, false, true, false);
      return true;

    case IDC_CONTENT_CONTEXT_COPY:
      *accel = ui::Accelerator(ui::VKEY_C, false, true, false);
      return true;

    case IDC_CONTENT_CONTEXT_PASTE:
      *accel = ui::Accelerator(ui::VKEY_V, false, true, false);
      return true;

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      *accel = ui::Accelerator(ui::VKEY_V, true, true, false);
      return true;

    case IDC_CONTENT_CONTEXT_SELECTALL:
      *accel = ui::Accelerator(ui::VKEY_A, false, true, false);
      return true;

    default:
      return false;
  }
}

void RenderViewContextMenuViews::UpdateMenuItem(int command_id,
                                                bool enabled,
                                                bool hidden,
                                                const string16& title) {
  views::MenuItemView* item = menu_->GetMenuItemByID(command_id);
  if (!item)
    return;

  item->SetEnabled(enabled);
  item->SetTitle(title);
  item->SetVisible(!hidden);

  views::MenuItemView* parent = item->GetParentMenuItem();
  if (!parent)
    return;

  parent->ChildrenChanged();
}
