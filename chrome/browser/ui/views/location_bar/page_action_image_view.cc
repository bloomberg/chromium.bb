// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/menu/menu_runner.h"

// static
const char PageActionImageView::kViewClassName[] = "PageActionImageView";

PageActionImageView::PageActionImageView(LocationBarView* owner,
                                         ExtensionAction* page_action,
                                         Browser* browser)
    : view_controller_(new ExtensionActionViewController(
          extensions::ExtensionRegistry::Get(browser->profile())->
              enabled_extensions().GetByID(page_action->extension_id()),
          browser,
          page_action,
          nullptr)),
      owner_(owner),
      preview_enabled_(false) {
  // There should be an associated focus manager so that we can safely register
  // accelerators for commands.
  DCHECK(GetFocusManagerForAccelerator());
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  view_controller_->SetDelegate(this);
  view_controller_->RegisterCommand();
  set_context_menu_controller(this);
}

PageActionImageView::~PageActionImageView() {
}

const char* PageActionImageView::GetClassName() const {
  return kViewClassName;
}

void PageActionImageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_BUTTON;
  node_data->SetName(tooltip_);
}

bool PageActionImageView::OnMousePressed(const ui::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.  (Also, triggering on mouse press causes bugs like
  // http://crbug.com/33155.)
  return true;
}

void PageActionImageView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!HitTestPoint(event.location()))
    return;

  if (event.IsRightMouseButton()) {
    // Don't show a menu here, its handled in View::ProcessMouseReleased. We
    // show the context menu by way of being the ContextMenuController.
    return;
  }

  view_controller_->ExecuteAction(true);
}

bool PageActionImageView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    view_controller_->ExecuteAction(true);
    return true;
  }
  return false;
}

void PageActionImageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    view_controller_->ExecuteAction(true);
    event->SetHandled();
  }
}

void PageActionImageView::UpdateVisibility(content::WebContents* contents) {
  int tab_id = SessionTabHelper::IdForTab(contents);
  if (!contents ||
      tab_id == -1 ||
      (!preview_enabled_ && !extension_action()->GetIsVisible(tab_id))) {
    SetVisible(false);
    return;
  }

  // Set the tooltip.
  tooltip_ = extension_action()->GetTitle(tab_id);
  SetTooltipText(base::UTF8ToUTF16(tooltip_));

  // Set the image.
  gfx::Size size(ExtensionAction::ActionIconSize(),
                 ExtensionAction::ActionIconSize());
  gfx::Image icon = view_controller_->GetIcon(contents, size);
  if (!icon.IsEmpty())
    SetImage(*icon.ToImageSkia());

  SetVisible(true);
}

void PageActionImageView::UpdateState() {
  UpdateVisibility(GetCurrentWebContents());
}

views::View* PageActionImageView::GetAsView() {
  return this;
}

bool PageActionImageView::IsMenuRunning() const {
  return menu_runner_.get() != nullptr;
}

views::FocusManager* PageActionImageView::GetFocusManagerForAccelerator() {
  return owner_->GetFocusManager();
}

views::View* PageActionImageView::GetReferenceViewForPopup() {
  return this;
}

content::WebContents* PageActionImageView::GetCurrentWebContents() const {
  return owner_->GetWebContents();
}

void PageActionImageView::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  ui::MenuModel* context_menu_model = view_controller_->GetContextMenu();
  // It's possible the action doesn't have a context menu.
  if (!context_menu_model)
    return;

  gfx::Point screen_loc;
  ConvertPointToScreen(this, &screen_loc);
  int run_types =
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;
  menu_runner_.reset(new views::MenuRunner(context_menu_model, run_types));

  if (menu_runner_->RunMenuAt(GetWidget(),
                              nullptr,  // No menu button for page action views.
                              gfx::Rect(screen_loc, size()),
                              views::MENU_ANCHOR_TOPLEFT,
                              source_type) == views::MenuRunner::MENU_DELETED) {
    return;
  }

  menu_runner_.reset();
  view_controller_->OnContextMenuClosed();
}
