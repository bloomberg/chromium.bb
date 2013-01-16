// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/script_bubble_icon_view.h"

#include "base/string_number_conversions.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/script_bubble_view.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/icon_with_badge_image_source.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

ScriptBubbleIconView::ScriptBubbleIconView(
    LocationBarView::Delegate* location_bar_delegate)
    : location_bar_delegate_(location_bar_delegate),
      script_count_(0) {
  set_id(VIEW_ID_SCRIPT_BUBBLE);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_SCRIPT_BUBBLE));
  set_accessibility_focusable(true);
  TouchableLocationBarView::Init(this);
}

ScriptBubbleIconView::~ScriptBubbleIconView() {
}

int ScriptBubbleIconView::GetBuiltInHorizontalPadding() const {
  return GetBuiltInHorizontalPaddingImpl();
}

void ScriptBubbleIconView::SetScriptCount(size_t script_count) {
  script_count_ = script_count;
  gfx::ImageSkia* icon =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_EXTENSIONS_SCRIPT_BUBBLE);
  gfx::Size requested_size(19, 19);  // Icon is only 16x16, too small to badge.

  gfx::ImageSkia image = gfx::ImageSkia(
      new IconWithBadgeImageSource(
          *icon,
          requested_size,
          gfx::Size(0, 2),
          base::IntToString(script_count_),
          SkColor(),
          SkColorSetRGB(0, 170, 0),
          extensions::ActionInfo::TYPE_PAGE),
      requested_size);

  SetImage(image);
}

void ScriptBubbleIconView::Layout() {
  SetScriptCount(script_count_);
}

void ScriptBubbleIconView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_STAR);
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
}

bool ScriptBubbleIconView::OnMousePressed(const ui::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void ScriptBubbleIconView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location()))
    ShowScriptBubble(this, location_bar_delegate_->GetWebContents());
}

bool ScriptBubbleIconView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    ShowScriptBubble(this, location_bar_delegate_->GetWebContents());
    return true;
  }
  return false;
}

void ScriptBubbleIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    ShowScriptBubble(this, location_bar_delegate_->GetWebContents());
    event->SetHandled();
  }
}

void ScriptBubbleIconView::ShowScriptBubble(views::View* anchor_view,
                                            WebContents* web_contents) {
  ScriptBubbleView* script_bubble = new ScriptBubbleView(anchor_view,
                                                         web_contents);
  views::BubbleDelegateView::CreateBubble(script_bubble);
  script_bubble->Show();
}
