// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "extensions/browser/extension_registry.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"

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
          this)),
      owner_(owner),
      preview_enabled_(false) {
  // There should be an associated focus manager so that we can safely register
  // accelerators for commands.
  DCHECK(GetFocusManagerForAccelerator());
  SetAccessibilityFocusable(true);
  view_controller_->RegisterCommand();
  set_context_menu_controller(view_controller_.get());
}

PageActionImageView::~PageActionImageView() {
}

const char* PageActionImageView::GetClassName() const {
  return kViewClassName;
}

void PageActionImageView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
  state->name = base::UTF8ToUTF16(tooltip_);
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

  view_controller_->ExecuteActionByUser();
}

bool PageActionImageView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    view_controller_->ExecuteActionByUser();
    return true;
  }
  return false;
}

void PageActionImageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    view_controller_->ExecuteActionByUser();
    event->SetHandled();
  }
}

void PageActionImageView::UpdateVisibility(content::WebContents* contents) {
  int tab_id = view_controller_->GetCurrentTabId();

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
  gfx::Image icon = view_controller_->GetIcon(tab_id);
  if (!icon.IsEmpty())
    SetImage(*icon.ToImageSkia());

  SetVisible(true);
}

void PageActionImageView::PaintChildren(gfx::Canvas* canvas,
                                        const views::CullSet& cull_set) {
  View::PaintChildren(canvas, cull_set);
  int tab_id = view_controller_->GetCurrentTabId();
  if (tab_id >= 0) {
    view_controller_->extension_action()->PaintBadge(
        canvas, GetLocalBounds(), tab_id);
  }
}

void PageActionImageView::OnIconUpdated() {
  UpdateVisibility(GetCurrentWebContents());
}

views::View* PageActionImageView::GetAsView() {
  return this;
}

bool PageActionImageView::IsShownInMenu() {
  return false;
}

views::FocusManager* PageActionImageView::GetFocusManagerForAccelerator() {
  return owner_->GetFocusManager();
}

views::Widget* PageActionImageView::GetParentForContextMenu() {
  return GetWidget();
}

ExtensionActionViewController*
PageActionImageView::GetPreferredPopupViewController() {
  return view_controller_.get();
}

views::View* PageActionImageView::GetReferenceViewForPopup() {
  return this;
}

views::MenuButton* PageActionImageView::GetContextMenuButton() {
  return NULL;  // No menu button for page action views.
}

content::WebContents* PageActionImageView::GetCurrentWebContents() {
  return owner_->GetWebContents();
}

void PageActionImageView::HideActivePopup() {
  // The only popup that will be active is this popup.
  view_controller_->HidePopup();
}
