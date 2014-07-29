// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"

#include <algorithm>

#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "grit/ui_resources.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/path.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/window_animations.h"

// This is the number of pixels in the border image interior to the actual
// border.
const int kBorderInterior = 6;

class OmniboxPopupContentsView::AutocompletePopupWidget
    : public views::Widget,
      public base::SupportsWeakPtr<AutocompletePopupWidget> {
 public:
  AutocompletePopupWidget() {}
  virtual ~AutocompletePopupWidget() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupWidget);
};

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, public:

OmniboxPopupView* OmniboxPopupContentsView::Create(
    const gfx::FontList& font_list,
    OmniboxView* omnibox_view,
    OmniboxEditModel* edit_model,
    LocationBarView* location_bar_view) {
  OmniboxPopupContentsView* view = NULL;
  view = new OmniboxPopupContentsView(
      font_list, omnibox_view, edit_model, location_bar_view);
  view->Init();
  return view;
}

OmniboxPopupContentsView::OmniboxPopupContentsView(
    const gfx::FontList& font_list,
    OmniboxView* omnibox_view,
    OmniboxEditModel* edit_model,
    LocationBarView* location_bar_view)
    : model_(new OmniboxPopupModel(this, edit_model)),
      omnibox_view_(omnibox_view),
      location_bar_view_(location_bar_view),
      font_list_(font_list),
      ignore_mouse_drag_(false),
      size_animation_(this),
      left_margin_(0),
      right_margin_(0),
      outside_vertical_padding_(0) {
  // The contents is owned by the LocationBarView.
  set_owned_by_client();

  ui::ThemeProvider* theme = location_bar_view_->GetThemeProvider();
  bottom_shadow_ = theme->GetImageSkiaNamed(IDR_BUBBLE_B);

  SetEventTargeter(
      scoped_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
}

void OmniboxPopupContentsView::Init() {
  // This can't be done in the constructor as at that point we aren't
  // necessarily our final class yet, and we may have subclasses
  // overriding CreateResultView.
  for (size_t i = 0; i < AutocompleteResult::kMaxMatches; ++i) {
    OmniboxResultView* result_view = CreateResultView(i, font_list_);
    result_view->SetVisible(false);
    AddChildViewAt(result_view, static_cast<int>(i));
  }
}

OmniboxPopupContentsView::~OmniboxPopupContentsView() {
  // We don't need to do anything with |popup_| here.  The OS either has already
  // closed the window, in which case it's been deleted, or it will soon, in
  // which case there's nothing we need to do.
}

gfx::Rect OmniboxPopupContentsView::GetPopupBounds() const {
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

void OmniboxPopupContentsView::LayoutChildren() {
  gfx::Rect contents_rect = GetContentsBounds();

  contents_rect.Inset(left_margin_,
                      views::NonClientFrameView::kClientEdgeThickness +
                          outside_vertical_padding_,
                      right_margin_, outside_vertical_padding_);
  int top = contents_rect.y();
  for (size_t i = 0; i < AutocompleteResult::kMaxMatches; ++i) {
    View* v = child_at(i);
    if (v->visible()) {
      v->SetBounds(contents_rect.x(), top, contents_rect.width(),
                   v->GetPreferredSize().height());
      top = v->bounds().bottom();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, OmniboxPopupView overrides:

bool OmniboxPopupContentsView::IsOpen() const {
  return popup_ != NULL;
}

void OmniboxPopupContentsView::InvalidateLine(size_t line) {
  OmniboxResultView* result = result_view_at(line);
  result->Invalidate();

  if (HasMatchAt(line) && GetMatchAtIndex(line).associated_keyword.get()) {
    result->ShowKeyword(IsSelectedIndex(line) &&
        model_->selected_line_state() == OmniboxPopupModel::KEYWORD);
  }
}

void OmniboxPopupContentsView::UpdatePopupAppearance() {
  const size_t hidden_matches = model_->result().ShouldHideTopMatch() ? 1 : 0;
  if (model_->result().size() <= hidden_matches ||
      omnibox_view_->IsImeShowingPopup()) {
    // No matches or the IME is showing a popup window which may overlap
    // the omnibox popup window.  Close any existing popup.
    if (popup_ != NULL) {
      size_animation_.Stop();

      // NOTE: Do NOT use CloseNow() here, as we may be deep in a callstack
      // triggered by the popup receiving a message (e.g. LBUTTONUP), and
      // destroying the popup would cause us to read garbage when we unwind back
      // to that level.
      popup_->Close();  // This will eventually delete the popup.
      popup_.reset();
    }
    return;
  }

  // Update the match cached by each row, in the process of doing so make sure
  // we have enough row views.
  const size_t result_size = model_->result().size();
  max_match_contents_width_ = 0;
  for (size_t i = 0; i < result_size; ++i) {
    OmniboxResultView* view = result_view_at(i);
    const AutocompleteMatch& match = GetMatchAtIndex(i);
    view->SetMatch(match);
    view->SetVisible(i >= hidden_matches);
    if (match.type == AutocompleteMatchType::SEARCH_SUGGEST_INFINITE) {
      max_match_contents_width_ = std::max(
          max_match_contents_width_, view->GetMatchContentsWidth());
    }
  }

  for (size_t i = result_size; i < AutocompleteResult::kMaxMatches; ++i)
    child_at(i)->SetVisible(false);

  gfx::Point top_left_screen_coord;
  int width;
  location_bar_view_->GetOmniboxPopupPositioningInfo(
      &top_left_screen_coord, &width, &left_margin_, &right_margin_);
  gfx::Rect new_target_bounds(top_left_screen_coord,
                              gfx::Size(width, CalculatePopupHeight()));

  // If we're animating and our target height changes, reset the animation.
  // NOTE: If we just reset blindly on _every_ update, then when the user types
  // rapidly we could get "stuck" trying repeatedly to animate shrinking by the
  // last few pixels to get to one visible result.
  if (new_target_bounds.height() != target_bounds_.height())
    size_animation_.Reset();
  target_bounds_ = new_target_bounds;

  if (popup_ == NULL) {
    views::Widget* popup_parent = location_bar_view_->GetWidget();

    // If the popup is currently closed, we need to create it.
    popup_ = (new AutocompletePopupWidget)->AsWeakPtr();
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.parent = popup_parent->GetNativeView();
    params.bounds = GetPopupBounds();
    params.context = popup_parent->GetNativeWindow();
    popup_->Init(params);
    // Third-party software such as DigitalPersona identity verification can
    // hook the underlying window creation methods and use SendMessage to
    // synchronously change focus/activation, resulting in the popup being
    // destroyed by the time control returns here.  Bail out in this case to
    // avoid a NULL dereference.
    if (!popup_.get())
      return;
    wm::SetWindowVisibilityAnimationTransition(
        popup_->GetNativeView(), wm::ANIMATE_NONE);
    popup_->SetContentsView(this);
    popup_->StackAbove(omnibox_view_->GetRelativeWindowForPopup());
    if (!popup_.get()) {
      // For some IMEs GetRelativeWindowForPopup triggers the omnibox to lose
      // focus, thereby closing (and destroying) the popup.
      // TODO(sky): this won't be needed once we close the omnibox on input
      // window showing.
      return;
    }
    popup_->ShowInactive();
  } else {
    // Animate the popup shrinking, but don't animate growing larger since that
    // would make the popup feel less responsive.
    start_bounds_ = GetWidget()->GetWindowBoundsInScreen();
    if (target_bounds_.height() < start_bounds_.height())
      size_animation_.Show();
    else
      start_bounds_ = target_bounds_;
    popup_->SetBounds(GetPopupBounds());
  }

  Layout();
}

gfx::Rect OmniboxPopupContentsView::GetTargetBounds() {
  return target_bounds_;
}

void OmniboxPopupContentsView::PaintUpdatesNow() {
  // TODO(beng): remove this from the interface.
}

void OmniboxPopupContentsView::OnDragCanceled() {
  ignore_mouse_drag_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, OmniboxResultViewModel implementation:

bool OmniboxPopupContentsView::IsSelectedIndex(size_t index) const {
  return index == model_->selected_line();
}

bool OmniboxPopupContentsView::IsHoveredIndex(size_t index) const {
  return index == model_->hovered_line();
}

gfx::Image OmniboxPopupContentsView::GetIconIfExtensionMatch(
    size_t index) const {
  if (!HasMatchAt(index))
    return gfx::Image();
  return model_->GetIconIfExtensionMatch(GetMatchAtIndex(index));
}

bool OmniboxPopupContentsView::IsStarredMatch(
    const AutocompleteMatch& match) const {
  return model_->IsStarredMatch(match);
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, AnimationDelegate implementation:

void OmniboxPopupContentsView::AnimationProgressed(
    const gfx::Animation* animation) {
  // We should only be running the animation when the popup is already visible.
  DCHECK(popup_ != NULL);
  popup_->SetBounds(GetPopupBounds());
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, views::View overrides:

void OmniboxPopupContentsView::Layout() {
  // Size our children to the available content area.
  LayoutChildren();

  // We need to manually schedule a paint here since we are a layered window and
  // won't implicitly require painting until we ask for one.
  SchedulePaint();
}

views::View* OmniboxPopupContentsView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return NULL;
}

bool OmniboxPopupContentsView::OnMousePressed(
    const ui::MouseEvent& event) {
  ignore_mouse_drag_ = false;  // See comment on |ignore_mouse_drag_| in header.
  if (event.IsLeftMouseButton() || event.IsMiddleMouseButton())
    UpdateLineEvent(event, event.IsLeftMouseButton());
  return true;
}

bool OmniboxPopupContentsView::OnMouseDragged(
    const ui::MouseEvent& event) {
  if (event.IsLeftMouseButton() || event.IsMiddleMouseButton())
    UpdateLineEvent(event, !ignore_mouse_drag_ && event.IsLeftMouseButton());
  return true;
}

void OmniboxPopupContentsView::OnMouseReleased(
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

void OmniboxPopupContentsView::OnMouseCaptureLost() {
  ignore_mouse_drag_ = false;
}

void OmniboxPopupContentsView::OnMouseMoved(
    const ui::MouseEvent& event) {
  model_->SetHoveredLine(GetIndexForPoint(event.location()));
}

void OmniboxPopupContentsView::OnMouseEntered(
    const ui::MouseEvent& event) {
  model_->SetHoveredLine(GetIndexForPoint(event.location()));
}

void OmniboxPopupContentsView::OnMouseExited(
    const ui::MouseEvent& event) {
  model_->SetHoveredLine(OmniboxPopupModel::kNoMatch);
}

void OmniboxPopupContentsView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_UPDATE:
      UpdateLineEvent(*event, true);
      break;
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_SCROLL_END:
      OpenSelectedLine(*event, CURRENT_TAB);
      break;
    default:
      return;
  }
  event->SetHandled();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, protected:

void OmniboxPopupContentsView::PaintResultViews(gfx::Canvas* canvas) {
  canvas->DrawColor(result_view_at(0)->GetColor(
      OmniboxResultView::NORMAL, OmniboxResultView::BACKGROUND));
  View::PaintChildren(canvas, views::CullSet());
}

int OmniboxPopupContentsView::CalculatePopupHeight() {
  DCHECK_GE(static_cast<size_t>(child_count()), model_->result().size());
  int popup_height = 0;
  for (size_t i = model_->result().ShouldHideTopMatch() ? 1 : 0;
       i < model_->result().size(); ++i)
    popup_height += child_at(i)->GetPreferredSize().height();

  // Add enough space on the top and bottom so it looks like there is the same
  // amount of space between the text and the popup border as there is in the
  // interior between each row of text.
  //
  // Discovering the exact amount of leading and padding around the font is
  // a bit tricky and platform-specific, but this computation seems to work in
  // practice.
  OmniboxResultView* result_view = result_view_at(0);
  outside_vertical_padding_ =
      (result_view->GetPreferredSize().height() -
       result_view->GetTextHeight());

  return popup_height +
         views::NonClientFrameView::kClientEdgeThickness +  // Top border.
         outside_vertical_padding_ * 2 +                    // Padding.
         bottom_shadow_->height() - kBorderInterior;        // Bottom border.
}

OmniboxResultView* OmniboxPopupContentsView::CreateResultView(
    int model_index,
    const gfx::FontList& font_list) {
  return new OmniboxResultView(this, model_index, location_bar_view_,
                               font_list);
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, views::View overrides, protected:

void OmniboxPopupContentsView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect contents_bounds = GetContentsBounds();
  contents_bounds.set_height(
      contents_bounds.height() - bottom_shadow_->height() + kBorderInterior);

  gfx::Path path;
  MakeContentsPath(&path, contents_bounds);
  canvas->Save();
  canvas->sk_canvas()->clipPath(path,
                                SkRegion::kIntersect_Op,
                                true /* doAntialias */);
  PaintResultViews(canvas);
  canvas->Restore();

  // Top border.
  canvas->FillRect(
      gfx::Rect(0, 0, width(), views::NonClientFrameView::kClientEdgeThickness),
      ThemeProperties::GetDefaultColor(
          ThemeProperties::COLOR_TOOLBAR_SEPARATOR));

  // Bottom border.
  canvas->TileImageInt(*bottom_shadow_, 0, height() - bottom_shadow_->height(),
                       width(), bottom_shadow_->height());
}

void OmniboxPopupContentsView::PaintChildren(gfx::Canvas* canvas,
                                             const views::CullSet& cull_set) {
  // We paint our children inside OnPaint().
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, private:

views::View* OmniboxPopupContentsView::TargetForRect(views::View* root,
                                                     const gfx::Rect& rect) {
  CHECK_EQ(root, this);
  return this;
}

bool OmniboxPopupContentsView::HasMatchAt(size_t index) const {
  return index < model_->result().size();
}

const AutocompleteMatch& OmniboxPopupContentsView::GetMatchAtIndex(
    size_t index) const {
  return model_->result().match_at(index);
}

void OmniboxPopupContentsView::MakeContentsPath(
    gfx::Path* path,
    const gfx::Rect& bounding_rect) {
  SkRect rect;
  rect.set(SkIntToScalar(bounding_rect.x()),
           SkIntToScalar(bounding_rect.y()),
           SkIntToScalar(bounding_rect.right()),
           SkIntToScalar(bounding_rect.bottom()));
  path->addRect(rect);
}

size_t OmniboxPopupContentsView::GetIndexForPoint(
    const gfx::Point& point) {
  if (!HitTestPoint(point))
    return OmniboxPopupModel::kNoMatch;

  int nb_match = model_->result().size();
  DCHECK(nb_match <= child_count());
  for (int i = 0; i < nb_match; ++i) {
    views::View* child = child_at(i);
    gfx::Point point_in_child_coords(point);
    View::ConvertPointToTarget(this, child, &point_in_child_coords);
    if (child->visible() && child->HitTestPoint(point_in_child_coords))
      return i;
  }
  return OmniboxPopupModel::kNoMatch;
}

void OmniboxPopupContentsView::UpdateLineEvent(
    const ui::LocatedEvent& event,
    bool should_set_selected_line) {
  size_t index = GetIndexForPoint(event.location());
  model_->SetHoveredLine(index);
  if (HasMatchAt(index) && should_set_selected_line)
    model_->SetSelectedLine(index, false, false);
}

void OmniboxPopupContentsView::OpenSelectedLine(
    const ui::LocatedEvent& event,
    WindowOpenDisposition disposition) {
  size_t index = GetIndexForPoint(event.location());
  if (!HasMatchAt(index))
    return;
  omnibox_view_->OpenMatch(model_->result().match_at(index), disposition,
                           GURL(), base::string16(), index);
}

OmniboxResultView* OmniboxPopupContentsView::result_view_at(size_t i) {
  return static_cast<OmniboxResultView*>(child_at(static_cast<int>(i)));
}
