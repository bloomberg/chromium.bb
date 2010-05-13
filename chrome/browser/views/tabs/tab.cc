// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab.h"

#include "app/l10n_util.h"
#include "app/menus/simple_menu_model.h"
#include "app/resource_bundle.h"
#include "base/compiler_specific.h"
#include "chrome/browser/tab_menu_model.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "gfx/canvas.h"
#include "gfx/font.h"
#include "gfx/path.h"
#include "gfx/size.h"
#include "grit/generated_resources.h"
#include "views/controls/menu/menu_2.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget.h"

const std::string Tab::kTabClassName = "browser/tabs/Tab";

static const SkScalar kTabCapWidth = 15;
static const SkScalar kTabTopCurveWidth = 4;
static const SkScalar kTabBottomCurveWidth = 3;

///////////////////////////////////////////////////////////////////////////////
// Tab, public:

Tab::Tab(TabController* controller)
    : TabRenderer(controller),
      closing_(false),
      dragging_(false) {
  close_button()->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_CLOSE_TAB));
  close_button()->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_CLOSE));
  close_button()->SetAnimationDuration(0);
  SetContextMenuController(this);
}

Tab::~Tab() {
}

///////////////////////////////////////////////////////////////////////////////
// Tab, TabRenderer overrides:

bool Tab::IsSelected() const {
  return controller() ? controller()->IsTabSelected(this) : true;
}

///////////////////////////////////////////////////////////////////////////////
// Tab, views::View overrides:

bool Tab::HasHitTestMask() const {
  return true;
}

void Tab::GetHitTestMask(gfx::Path* mask) const {
  MakePathForTab(mask);
}

bool Tab::OnMousePressed(const views::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    // Store whether or not we were selected just now... we only want to be
    // able to drag foreground tabs, so we don't start dragging the tab if
    // it was in the background.
    bool just_selected = !IsSelected();
    if (just_selected) {
      controller()->SelectTab(this);
    }
    controller()->MaybeStartDrag(this, event);
  }
  return true;
}

bool Tab::OnMouseDragged(const views::MouseEvent& event) {
  controller()->ContinueDrag(event);
  return true;
}

void Tab::OnMouseReleased(const views::MouseEvent& event, bool canceled) {
  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  // In some cases, ending the drag will schedule the tab for destruction; if
  // so, bail immediately, since our members are already dead and we shouldn't
  // do anything else except drop the tab where it is.
  if (controller()->EndDrag(canceled))
    return;

  // Close tab on middle click, but only if the button is released over the tab
  // (normal windows behavior is to discard presses of a UI element where the
  // releases happen off the element).
  if (event.IsMiddleMouseButton() && HitTest(event.location()))
    controller()->CloseTab(this);
}

bool Tab::GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
  std::wstring title = GetTitle();
  if (!title.empty()) {
    // Only show the tooltip if the title is truncated.
    gfx::Font font;
    if (font.GetStringWidth(title) > title_bounds().width()) {
      *tooltip = title;
      return true;
    }
  }
  return false;
}

bool Tab::GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* origin) {
  gfx::Font font;
  origin->set_x(title_bounds().x() + 10);
  origin->set_y(-views::TooltipManager::GetTooltipHeight() - 4);
  return true;
}

bool Tab::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_PAGETAB;
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Tab, views::ContextMenuController implementation:

void Tab::ShowContextMenu(views::View* source,
                          const gfx::Point& p,
                          bool is_mouse_gesture) {
  controller()->ShowContextMenu(this, p);
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation:

void Tab::ButtonPressed(views::Button* sender, const views::Event& event) {
  if (sender == close_button())
    controller()->CloseTab(this);
}

///////////////////////////////////////////////////////////////////////////////
// Tab, private:

void Tab::MakePathForTab(gfx::Path* path) const {
  DCHECK(path);

  SkScalar h = SkIntToScalar(height());
  SkScalar w = SkIntToScalar(width());

  path->moveTo(0, h);

  // Left end cap.
  path->lineTo(kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(kTabCapWidth, 0);

  // Connect to the right cap.
  path->lineTo(w - kTabCapWidth, 0);

  // Right end cap.
  path->lineTo(w - kTabCapWidth + kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(w - kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(w, h);

  // Close out the path.
  path->lineTo(0, h);
  path->close();
}
