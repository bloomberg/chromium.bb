// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/renderer_context_menu/render_view_context_menu_views.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "chrome/app/chrome_command_ids.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/point.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

using content::WebContents;

namespace {

// Views implementation of the ToolkitDelegate.
// TODO(oshima): Move this to components/renderer_context_menu/
class ToolkitDelegateViews : public RenderViewContextMenu::ToolkitDelegate {
 public:
  ToolkitDelegateViews() : menu_view_(NULL) {}

  virtual ~ToolkitDelegateViews() {}

  void RunMenuAt(views::Widget* parent,
                 const gfx::Point& point,
                 ui::MenuSourceType type) {
    views::MenuAnchorPosition anchor_position =
        (type == ui::MENU_SOURCE_TOUCH ||
         type == ui::MENU_SOURCE_TOUCH_EDIT_MENU)
        ? views::MENU_ANCHOR_BOTTOMCENTER
        : views::MENU_ANCHOR_TOPLEFT;
    views::MenuRunner::RunResult result ALLOW_UNUSED = menu_runner_->RunMenuAt(
        parent, NULL, gfx::Rect(point, gfx::Size()), anchor_position, type);
  }

 private:
  // ToolkitDelegate:
  virtual void Init(ui::SimpleMenuModel* menu_model) OVERRIDE {
    menu_adapter_.reset(new views::MenuModelAdapter(menu_model));
    menu_view_ = menu_adapter_->CreateMenu();
    menu_runner_.reset(new views::MenuRunner(
        menu_view_,
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
  }

  virtual void Cancel() OVERRIDE{
    DCHECK(menu_runner_.get());
    menu_runner_->Cancel();
  }

  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const base::string16& title) OVERRIDE {
    views::MenuItemView* item = menu_view_->GetMenuItemByID(command_id);
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

  scoped_ptr<views::MenuModelAdapter> menu_adapter_;
  scoped_ptr<views::MenuRunner> menu_runner_;

  // Weak. Owned by menu_runner_;
  views::MenuItemView* menu_view_;

  DISALLOW_COPY_AND_ASSIGN(ToolkitDelegateViews);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuViews, public:

RenderViewContextMenuViews::RenderViewContextMenuViews(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params)
    : RenderViewContextMenu(render_frame_host, params),
      bidi_submenu_model_(this) {
  scoped_ptr<ToolkitDelegate> delegate(new ToolkitDelegateViews);
  set_toolkit_delegate(delegate.Pass());
}

RenderViewContextMenuViews::~RenderViewContextMenuViews() {
}

// static
RenderViewContextMenuViews* RenderViewContextMenuViews::Create(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  return new RenderViewContextMenuViews(render_frame_host, params);
}

void RenderViewContextMenuViews::RunMenuAt(views::Widget* parent,
                                           const gfx::Point& point,
                                           ui::MenuSourceType type) {
  static_cast<ToolkitDelegateViews*>(toolkit_delegate())->
      RunMenuAt(parent, point, type);
}

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuViews, protected:

bool RenderViewContextMenuViews::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accel) {
  // There are no formally defined accelerators we can query so we assume
  // that Ctrl+C, Ctrl+V, Ctrl+X, Ctrl-A, etc do what they normally do.
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_UNDO:
      *accel = ui::Accelerator(ui::VKEY_Z, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_REDO:
      // TODO(jcampan): should it be Ctrl-Y?
      *accel = ui::Accelerator(ui::VKEY_Z,
                               ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_CUT:
      *accel = ui::Accelerator(ui::VKEY_X, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_COPY:
      *accel = ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_PASTE:
      *accel = ui::Accelerator(ui::VKEY_V, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      *accel = ui::Accelerator(ui::VKEY_V,
                               ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_SELECTALL:
      *accel = ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN);
      return true;

    default:
      return false;
  }
}

void RenderViewContextMenuViews::ExecuteCommand(int command_id,
                                                int event_flags) {
  switch (command_id) {
    case IDC_WRITING_DIRECTION_DEFAULT:
      // WebKit's current behavior is for this menu item to always be disabled.
      NOTREACHED();
      break;

    case IDC_WRITING_DIRECTION_RTL:
    case IDC_WRITING_DIRECTION_LTR: {
      content::RenderViewHost* view_host = GetRenderViewHost();
      view_host->UpdateTextDirection((command_id == IDC_WRITING_DIRECTION_RTL) ?
          blink::WebTextDirectionRightToLeft :
          blink::WebTextDirectionLeftToRight);
      view_host->NotifyTextDirection();
      break;
    }

    default:
      RenderViewContextMenu::ExecuteCommand(command_id, event_flags);
      break;
  }
}

bool RenderViewContextMenuViews::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case IDC_WRITING_DIRECTION_DEFAULT:
      return (params_.writing_direction_default &
          blink::WebContextMenuData::CheckableMenuItemChecked) != 0;
    case IDC_WRITING_DIRECTION_RTL:
      return (params_.writing_direction_right_to_left &
          blink::WebContextMenuData::CheckableMenuItemChecked) != 0;
    case IDC_WRITING_DIRECTION_LTR:
      return (params_.writing_direction_left_to_right &
          blink::WebContextMenuData::CheckableMenuItemChecked) != 0;

    default:
      return RenderViewContextMenu::IsCommandIdChecked(command_id);
  }
}

bool RenderViewContextMenuViews::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDC_WRITING_DIRECTION_MENU:
      return true;
    case IDC_WRITING_DIRECTION_DEFAULT:  // Provided to match OS defaults.
      return params_.writing_direction_default &
          blink::WebContextMenuData::CheckableMenuItemEnabled;
    case IDC_WRITING_DIRECTION_RTL:
      return params_.writing_direction_right_to_left &
          blink::WebContextMenuData::CheckableMenuItemEnabled;
    case IDC_WRITING_DIRECTION_LTR:
      return params_.writing_direction_left_to_right &
          blink::WebContextMenuData::CheckableMenuItemEnabled;

    default:
      return RenderViewContextMenu::IsCommandIdEnabled(command_id);
  }
}

void RenderViewContextMenuViews::AppendPlatformEditableItems() {
  bidi_submenu_model_.AddCheckItem(
      IDC_WRITING_DIRECTION_DEFAULT,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_DEFAULT));
  bidi_submenu_model_.AddCheckItem(
      IDC_WRITING_DIRECTION_LTR,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_LTR));
  bidi_submenu_model_.AddCheckItem(
      IDC_WRITING_DIRECTION_RTL,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_RTL));

  menu_model_.AddSubMenu(
      IDC_WRITING_DIRECTION_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_MENU),
      &bidi_submenu_model_);
}
