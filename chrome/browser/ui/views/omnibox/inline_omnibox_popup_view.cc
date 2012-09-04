// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/inline_omnibox_popup_view.h"

#include "base/compiler_specific.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/webui/instant_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/path.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"
#include "unicode/ubidi.h"

namespace {

// The size delta between the font used for the edit and the result rows. Passed
// to gfx::Font::DeriveFont.
#if defined(OS_CHROMEOS)
// Don't adjust the size on Chrome OS (http://crbug.com/61433).
const int kEditFontAdjust = 0;
#else
const int kEditFontAdjust = -1;
#endif

const int kRetractDurationMs = 120;

}  // namespace

InlineOmniboxPopupView::InlineOmniboxPopupView(
    const gfx::Font& font,
    OmniboxView* omnibox_view,
    OmniboxEditModel* edit_model,
    views::View* location_bar)
    : model_(new OmniboxPopupModel(this, edit_model)),
      omnibox_view_(omnibox_view),
      profile_(edit_model->profile()),
      location_bar_(location_bar),
      result_font_(font.DeriveFont(kEditFontAdjust)),
      result_bold_font_(result_font_.DeriveFont(0, gfx::Font::BOLD)),
      ignore_mouse_drag_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(size_animation_(this)) {
  // We're owned by LocationBarView.
  set_owned_by_client();
  // Our visibility determines whether the popup is open. Start out closed.
  SetVisible(false);
  set_background(views::Background::CreateSolidBackground(
                     chrome::search::kSuggestBackgroundColor));
  size_animation_.SetSlideDuration(kRetractDurationMs *
                                   InstantUI::GetSlowAnimationScaleFactor());
  size_animation_.SetTweenType(ui::Tween::EASE_IN_OUT);
}

InlineOmniboxPopupView::~InlineOmniboxPopupView() {
}

void InlineOmniboxPopupView::Init() {
  // This can't be done in the constructor as at that point we aren't
  // necessarily our final class yet, and we may have subclasses
  // overriding CreateResultView.
  for (size_t i = 0; i < AutocompleteResult::kMaxMatches; ++i) {
    OmniboxResultView* result_view =
        CreateResultView(this, i, result_font_, result_bold_font_);
    result_view->SetVisible(false);
    AddChildViewAt(result_view, static_cast<int>(i));
  }
}

gfx::Rect InlineOmniboxPopupView::GetPopupBounds() const {
  if (!size_animation_.is_animating())
    return target_bounds_;

  gfx::Rect current_frame_bounds = start_bounds_;
  int total_height_delta = target_bounds_.height() - start_bounds_.height();
  // Round |current_height_delta| instead of truncating so we won't leave single
  // white pixels at the bottom of the popup as long when animating very small
  // height differences.
  int current_height_delta = static_cast<int>(
      size_animation_.GetCurrentValue() * total_height_delta - 0.5);
  current_frame_bounds.set_height(
      current_frame_bounds.height() + current_height_delta);
  return current_frame_bounds;
}

void InlineOmniboxPopupView::LayoutChildren() {
  // Make the children line up with the LocationBar.
  gfx::Point content_origin;
  // TODO(sky): this won't work correctly with LocationBarContainer.
  views::View::ConvertPointToTarget(location_bar_, this, &content_origin);
  int x = content_origin.x();
  int width = location_bar_->width();
  gfx::Rect contents_rect = GetContentsBounds();
  int top = contents_rect.y();
  for (int i = 0; i < child_count(); ++i) {
    View* v = child_at(i);
    if (v->visible()) {
      v->SetBounds(x, top, width, v->GetPreferredSize().height());
      top = v->bounds().bottom();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// InlineOmniboxPopupView, AutocompletePopupView overrides:

bool InlineOmniboxPopupView::IsOpen() const {
  return model_->result().empty() ? false : visible();
}

void InlineOmniboxPopupView::InvalidateLine(size_t line) {
  OmniboxResultView* result = static_cast<OmniboxResultView*>(
      child_at(static_cast<int>(line)));
  result->Invalidate();

  if (HasMatchAt(line) && GetMatchAtIndex(line).associated_keyword.get()) {
    result->ShowKeyword(IsSelectedIndex(line) &&
        model_->selected_line_state() == OmniboxPopupModel::KEYWORD);
  }
}

void InlineOmniboxPopupView::UpdatePopupAppearance() {
  gfx::Rect new_target_bounds;
  if (!model_->result().empty()) {
    // Update the match cached by each row, in the process of doing so make sure
    // we have enough row views.
    size_t child_rv_count = child_count();
    const size_t result_size = model_->result().size();
    for (size_t i = 0; i < result_size; ++i) {
      OmniboxResultView* view = static_cast<OmniboxResultView*>(child_at(i));
      view->SetMatch(GetMatchAtIndex(i));
      view->SetVisible(true);
    }
    for (size_t i = result_size; i < child_rv_count; ++i)
      child_at(i)->SetVisible(false);

    new_target_bounds = CalculateTargetBounds(CalculatePopupHeight());
  }

  // If we're animating and our target height changes, reset the animation.
  // NOTE: If we just reset blindly on _every_ update, then when the user types
  // rapidly we could get "stuck" trying repeatedly to animate shrinking by the
  // last few pixels to get to one visible result.
  if (new_target_bounds.height() != target_bounds_.height()) {
    size_animation_.Reset();
    PreferredSizeChanged();
  }
  target_bounds_ = new_target_bounds;

  if (!visible()) {
    SetVisible(true);
    PreferredSizeChanged();
  } else {
    // Animate the popup shrinking, but don't animate growing larger since that
    // would make the popup feel less responsive.
    start_bounds_ = bounds();
    if (target_bounds_.height() < start_bounds_.height())
      size_animation_.Show();
    else
      start_bounds_ = target_bounds_;
    if (GetPopupBounds().height() != bounds().height())
      PreferredSizeChanged();
  }

  SchedulePaint();
}

gfx::Rect InlineOmniboxPopupView::GetTargetBounds() {
  // Our bounds never obscure the page, so we return an empty rect.
  return gfx::Rect();
}

void InlineOmniboxPopupView::PaintUpdatesNow() {
  // TODO(beng): remove this from the interface.
}

void InlineOmniboxPopupView::OnDragCanceled() {
  ignore_mouse_drag_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// InlineOmniboxPopupView, OmniboxResultViewModel implementation:

bool InlineOmniboxPopupView::IsSelectedIndex(size_t index) const {
  return index == model_->selected_line();
}

bool InlineOmniboxPopupView::IsHoveredIndex(size_t index) const {
  return index == model_->hovered_line();
}

gfx::Image InlineOmniboxPopupView::GetIconIfExtensionMatch(
    size_t index) const {
  if (!HasMatchAt(index))
    return gfx::Image();
  return model_->GetIconIfExtensionMatch(GetMatchAtIndex(index));
}

////////////////////////////////////////////////////////////////////////////////
// InlineOmniboxPopupView, AnimationDelegate implementation:

void InlineOmniboxPopupView::AnimationProgressed(
    const ui::Animation* animation) {
  // We should only be running the animation when the popup is already visible.
  DCHECK(visible());
  PreferredSizeChanged();
}

void InlineOmniboxPopupView::AnimationEnded(
    const ui::Animation* animation) {
  if (model_->result().empty()) {
    SetVisible(false);
    PreferredSizeChanged();
  }
}

////////////////////////////////////////////////////////////////////////////////
// InlineOmniboxPopupView, views::View overrides:

gfx::Size InlineOmniboxPopupView::GetPreferredSize() {
  return GetPopupBounds().size();
}

void InlineOmniboxPopupView::Layout() {
  // Size our children to the available content area.
  LayoutChildren();
}

views::View* InlineOmniboxPopupView::GetEventHandlerForPoint(
    const gfx::Point& point) {
  return this;
}

bool InlineOmniboxPopupView::OnMousePressed(
    const ui::MouseEvent& event) {
  ignore_mouse_drag_ = false;  // See comment on |ignore_mouse_drag_| in header.
  if (event.IsLeftMouseButton() || event.IsMiddleMouseButton())
    UpdateLineEvent(event, event.IsLeftMouseButton());
  return true;
}

bool InlineOmniboxPopupView::OnMouseDragged(
    const ui::MouseEvent& event) {
  if (event.IsLeftMouseButton() || event.IsMiddleMouseButton())
    UpdateLineEvent(event, !ignore_mouse_drag_ && event.IsLeftMouseButton());
  return true;
}

void InlineOmniboxPopupView::OnMouseReleased(
    const ui::MouseEvent& event) {
  if (ignore_mouse_drag_) {
    OnMouseCaptureLost();
    return;
  }

  if (event.IsOnlyMiddleMouseButton() || event.IsOnlyLeftMouseButton()) {
    OpenSelectedLine(event, event.IsOnlyLeftMouseButton() ? CURRENT_TAB :
                                                            NEW_BACKGROUND_TAB);
  }
}

void InlineOmniboxPopupView::OnMouseCaptureLost() {
  ignore_mouse_drag_ = false;
}

void InlineOmniboxPopupView::OnMouseMoved(
    const ui::MouseEvent& event) {
  model_->SetHoveredLine(GetIndexForPoint(event.location()));
}

void InlineOmniboxPopupView::OnMouseEntered(
    const ui::MouseEvent& event) {
  model_->SetHoveredLine(GetIndexForPoint(event.location()));
}

void InlineOmniboxPopupView::OnMouseExited(
    const ui::MouseEvent& event) {
  model_->SetHoveredLine(OmniboxPopupModel::kNoMatch);
}

ui::EventResult InlineOmniboxPopupView::OnGestureEvent(
    const ui::GestureEvent& event) {
  switch (event.type()) {
    case ui::ET_GESTURE_TAP_DOWN:
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_UPDATE:
      UpdateLineEvent(event, true);
      break;
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_SCROLL_END:
      OpenSelectedLine(event, CURRENT_TAB);
      break;
    default:
      return ui::ER_UNHANDLED;
  }
  return ui::ER_CONSUMED;
}

////////////////////////////////////////////////////////////////////////////////
// InlineOmniboxPopupView, protected:

int InlineOmniboxPopupView::CalculatePopupHeight() {
  DCHECK_GE(static_cast<size_t>(child_count()), model_->result().size());
  int popup_height = 0;
  for (size_t i = 0; i < model_->result().size(); ++i)
    popup_height += child_at(i)->GetPreferredSize().height();
  return popup_height;
}

OmniboxResultView* InlineOmniboxPopupView::CreateResultView(
    OmniboxResultViewModel* model,
    int model_index,
    const gfx::Font& font,
    const gfx::Font& bold_font) {
  return new OmniboxResultView(model, model_index, font, bold_font);
}

////////////////////////////////////////////////////////////////////////////////
// InlineOmniboxPopupView, private:

bool InlineOmniboxPopupView::HasMatchAt(size_t index) const {
  return index < model_->result().size();
}

const AutocompleteMatch& InlineOmniboxPopupView::GetMatchAtIndex(
    size_t index) const {
  return model_->result().match_at(index);
}

void InlineOmniboxPopupView::MakeContentsPath(
    gfx::Path* path,
    const gfx::Rect& bounding_rect) {
  SkRect rect;
  rect.set(SkIntToScalar(bounding_rect.x()),
           SkIntToScalar(bounding_rect.y()),
           SkIntToScalar(bounding_rect.right()),
           SkIntToScalar(bounding_rect.bottom()));

  SkScalar radius = SkIntToScalar(views::BubbleBorder::GetCornerRadius());
  path->addRoundRect(rect, radius, radius);
}

void InlineOmniboxPopupView::OpenIndex(size_t index,
                                       WindowOpenDisposition disposition) {
  if (!HasMatchAt(index))
    return;

  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  AutocompleteMatch match = model_->result().match_at(index);
  omnibox_view_->OpenMatch(match, disposition, GURL(), index);
}

size_t InlineOmniboxPopupView::GetIndexForPoint(const gfx::Point& point) {
  if (!HitTestPoint(point))
    return OmniboxPopupModel::kNoMatch;

  int nb_match = model_->result().size();
  DCHECK(nb_match <= child_count());
  for (int i = 0; i < nb_match; ++i) {
    views::View* child = child_at(i);
    gfx::Point point_in_child_coords(point);
    View::ConvertPointToTarget(this, child, &point_in_child_coords);
    if (child->HitTestPoint(point_in_child_coords))
      return i;
  }
  return OmniboxPopupModel::kNoMatch;
}

gfx::Rect InlineOmniboxPopupView::CalculateTargetBounds(int h) {
  return gfx::Rect(0, 0, 1, h);
}

void InlineOmniboxPopupView::UpdateLineEvent(
    const ui::LocatedEvent& event,
    bool should_set_selected_line) {
  size_t index = GetIndexForPoint(event.location());
  model_->SetHoveredLine(index);
  if (HasMatchAt(index) && should_set_selected_line)
    model_->SetSelectedLine(index, false, false);
}

void InlineOmniboxPopupView::OpenSelectedLine(
    const ui::LocatedEvent& event,
    WindowOpenDisposition disposition) {
  size_t index = GetIndexForPoint(event.location());
  OpenIndex(index, disposition);
}
