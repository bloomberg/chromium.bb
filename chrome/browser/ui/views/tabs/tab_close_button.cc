// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_close_button.h"

#include <map>
#include <memory>
#include <vector>

#include "base/hash.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/rect_based_targeting_utils.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace {

const gfx::VectorIcon& GetTouchIcon(views::Button::ButtonState state,
                                    bool is_incognito) {
  if (state == views::Button::STATE_NORMAL)
    return kTabCloseButtonTouchIcon;
  return is_incognito ? kTabCloseButtonTouchIncognitoHoveredPressedIcon
                      : kTabCloseButtonTouchHoveredPressedIcon;
}

}  // namespace

TabCloseButton::TabCloseButton(views::ButtonListener* listener,
                               MouseEventCallback mouse_event_callback)
    : views::ImageButton(listener),
      mouse_event_callback_(std::move(mouse_event_callback)) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));

  if (!ui::MaterialDesignController::IsTouchOptimizedUiEnabled()) {
    // The normal image is set by OnButtonColorMaybeChanged() because it depends
    // on the current theme and active state.  The hovered and pressed images
    // don't depend on the these, so we can set them here.
    const gfx::ImageSkia& hovered = gfx::CreateVectorIcon(
        kTabCloseHoveredPressedIcon, SkColorSetRGB(0xDB, 0x44, 0x37));
    const gfx::ImageSkia& pressed = gfx::CreateVectorIcon(
        kTabCloseHoveredPressedIcon, SkColorSetRGB(0xA8, 0x35, 0x2A));
    SetImage(views::Button::STATE_HOVERED, &hovered);
    SetImage(views::Button::STATE_PRESSED, &pressed);
  }

  // Disable animation so that the red danger sign shows up immediately
  // to help avoid mis-clicks.
  SetAnimationDuration(0);
}

TabCloseButton::~TabCloseButton() {}

void TabCloseButton::SetTabColor(SkColor color) {
  const gfx::ImageSkia& close_button_normal_image =
      gfx::CreateVectorIcon(kTabCloseNormalIcon, color);
  SetImage(views::Button::STATE_NORMAL, &close_button_normal_image);
}

void TabCloseButton::ActiveStateChanged(const Tab* parent_tab) {
  constexpr views::Button::ButtonState states[] = {
      views::Button::STATE_NORMAL, views::Button::STATE_HOVERED,
      views::Button::STATE_PRESSED};
  const bool is_incognito = parent_tab->controller()->IsIncognito();

  // The tab close button icon has 8 combination of colors depending on what
  // state the button is in, and whether this is an incognito browsing session.
  // The 8 combination is achieved by creating 3 vector .icon files and setting
  // the colors for them here.
  for (const auto& state : states) {
    SetImage(state,
             gfx::CreateVectorIcon(GetTouchIcon(state, is_incognito),
                                   parent_tab->GetCloseTabButtonColor(state)));
  }
}

views::View* TabCloseButton::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  // Tab close button has no children, so tooltip handler should be the same
  // as the event handler. In addition, a hit test has to be performed for the
  // point (as GetTooltipHandlerForPoint() is responsible for it).
  if (!HitTestPoint(point))
    return nullptr;
  return GetEventHandlerForPoint(point);
}

bool TabCloseButton::OnMousePressed(const ui::MouseEvent& event) {
  mouse_event_callback_.Run(this, event);

  bool handled = ImageButton::OnMousePressed(event);
  // Explicitly mark midle-mouse clicks as non-handled to ensure the tab
  // sees them.
  return !event.IsMiddleMouseButton() && handled;
}

void TabCloseButton::OnMouseMoved(const ui::MouseEvent& event) {
  mouse_event_callback_.Run(this, event);
  Button::OnMouseMoved(event);
}

void TabCloseButton::OnMouseReleased(const ui::MouseEvent& event) {
  mouse_event_callback_.Run(this, event);
  Button::OnMouseReleased(event);
}

void TabCloseButton::OnGestureEvent(ui::GestureEvent* event) {
  // Consume all gesture events here so that the parent (Tab) does not
  // start consuming gestures.
  ImageButton::OnGestureEvent(event);
  event->SetHandled();
}

const char* TabCloseButton::GetClassName() const {
  return "TabCloseButton";
}

views::View* TabCloseButton::TargetForRect(views::View* root,
                                           const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  if (!views::UsePointBasedTargeting(rect))
    return ViewTargeterDelegate::TargetForRect(root, rect);

  // Ignore the padding set on the button.
  gfx::Rect contents_bounds = GetMirroredRect(GetContentsBounds());

#if defined(USE_AURA)
  // Include the padding in hit-test for touch events.
  // TODO(pkasting): It seems like touch events would generate rects rather
  // than points and thus use the TargetForRect() call above.  If this is
  // reached, it may be from someone calling GetEventHandlerForPoint() while a
  // touch happens to be occurring.  In such a case, maybe we don't want this
  // code to run?  It's possible this block should be removed, or maybe this
  // whole function deleted.  Note that in these cases, we should probably
  // also remove the padding on the close button bounds (see Tab::Layout()),
  // as it will be pointless.
  if (aura::Env::GetInstance()->is_touch_down())
    contents_bounds = GetLocalBounds();
#endif

  return contents_bounds.Intersects(rect) ? this : parent();
}

bool TabCloseButton::GetHitTestMask(gfx::Path* mask) const {
  // We need to define this so hit-testing won't include the border region.
  mask->addRect(gfx::RectToSkRect(GetMirroredRect(GetContentsBounds())));
  return true;
}
