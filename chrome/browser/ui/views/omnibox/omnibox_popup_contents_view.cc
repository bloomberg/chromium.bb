// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"

#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_non_view.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/touch_omnibox_popup_contents_view.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/path.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/views/corewm/window_animations.h"
#endif

#if defined(OS_WIN)
#include <dwmapi.h>

#include "base/win/scoped_gdi_object.h"
#if !defined(USE_AURA)
#include "ui/base/win/shell.h"
#endif
#endif

namespace {

const SkAlpha kGlassPopupAlpha = 240;
const SkAlpha kOpaquePopupAlpha = 255;

// The size delta between the font used for the edit and the result rows. Passed
// to gfx::Font::DeriveFont.
#if defined(OS_CHROMEOS)
// Don't adjust the size on Chrome OS (http://crbug.com/61433).
const int kEditFontAdjust = 0;
#else
const int kEditFontAdjust = -1;
#endif

}  // namespace

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
    const gfx::Font& font,
    OmniboxView* omnibox_view,
    OmniboxEditModel* edit_model,
    views::View* location_bar) {
  if (chrome::IsInstantExtendedAPIEnabled())
    return new OmniboxPopupNonView(edit_model);

  OmniboxPopupContentsView* view = NULL;
  if (ui::GetDisplayLayout() == ui::LAYOUT_TOUCH) {
    view = new TouchOmniboxPopupContentsView(
        font, omnibox_view, edit_model, location_bar);
  } else {
    view = new OmniboxPopupContentsView(
        font, omnibox_view, edit_model, location_bar);
  }

  view->Init();
  return view;
}

OmniboxPopupContentsView::OmniboxPopupContentsView(
    const gfx::Font& font,
    OmniboxView* omnibox_view,
    OmniboxEditModel* edit_model,
    views::View* location_bar)
    : model_(new OmniboxPopupModel(this, edit_model)),
      omnibox_view_(omnibox_view),
      location_bar_(location_bar),
      font_(font.DeriveFont(kEditFontAdjust)),
      ignore_mouse_drag_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(size_animation_(this)) {
  bubble_border_ = new views::BubbleBorder(views::BubbleBorder::NONE,
      views::BubbleBorder::NO_SHADOW, SK_ColorWHITE);
  set_border(const_cast<views::BubbleBorder*>(bubble_border_));
  // The contents is owned by the LocationBarView.
  set_owned_by_client();
}

void OmniboxPopupContentsView::Init() {
  // This can't be done in the constructor as at that point we aren't
  // necessarily our final class yet, and we may have subclasses
  // overriding CreateResultView.
  for (size_t i = 0; i < AutocompleteResult::kMaxMatches; ++i) {
    OmniboxResultView* result_view = CreateResultView(this, i, font_);
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
  int top = contents_rect.y();
  for (int i = 0; i < child_count(); ++i) {
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
  if (model_->result().empty()) {
    // No matches, close any existing popup.
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
  size_t child_rv_count = child_count();
  const size_t result_size = model_->result().size();
  for (size_t i = 0; i < result_size; ++i) {
    OmniboxResultView* view = result_view_at(i);
    view->SetMatch(GetMatchAtIndex(i));
    view->SetVisible(true);
  }
  for (size_t i = result_size; i < child_rv_count; ++i)
    child_at(i)->SetVisible(false);

  gfx::Rect new_target_bounds = CalculateTargetBounds(CalculatePopupHeight());

  // If we're animating and our target height changes, reset the animation.
  // NOTE: If we just reset blindly on _every_ update, then when the user types
  // rapidly we could get "stuck" trying repeatedly to animate shrinking by the
  // last few pixels to get to one visible result.
  if (new_target_bounds.height() != target_bounds_.height())
    size_animation_.Reset();
  target_bounds_ = new_target_bounds;

  if (popup_ == NULL) {
    // If the popup is currently closed, we need to create it.
    popup_ = (new AutocompletePopupWidget)->AsWeakPtr();
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.can_activate = false;
    params.transparent = true;
    params.parent = location_bar_->GetWidget()->GetNativeView();
    params.bounds = GetPopupBounds();
    params.context = location_bar_->GetWidget()->GetNativeView();
    popup_->Init(params);
#if defined(USE_AURA)
    views::corewm::SetWindowVisibilityAnimationType(
        popup_->GetNativeView(),
        views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
#if defined(OS_CHROMEOS)
    // No animation for autocomplete popup appearance.
    views::corewm::SetWindowVisibilityAnimationTransition(
        popup_->GetNativeView(), views::corewm::ANIMATE_HIDE);
#endif
#endif
    popup_->SetContentsView(this);
    popup_->StackAbove(omnibox_view_->GetRelativeWindowForPopup());
    if (!popup_.get()) {
      // For some IMEs GetRelativeWindowForPopup triggers the omnibox to lose
      // focus, thereby closing (and destroying) the popup.
      // TODO(sky): this won't be needed once we close the omnibox on input
      // window showing.
      return;
    }
    popup_->Show();
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

  SchedulePaint();
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

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, AnimationDelegate implementation:

void OmniboxPopupContentsView::AnimationProgressed(
    const ui::Animation* animation) {
  // We should only be running the animation when the popup is already visible.
  DCHECK(popup_ != NULL);
  popup_->SetBounds(GetPopupBounds());
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, views::View overrides:

void OmniboxPopupContentsView::Layout() {
  UpdateBlurRegion();

  // Size our children to the available content area.
  LayoutChildren();

  // We need to manually schedule a paint here since we are a layered window and
  // won't implicitly require painting until we ask for one.
  SchedulePaint();
}

views::View* OmniboxPopupContentsView::GetEventHandlerForPoint(
    const gfx::Point& point) {
  return this;
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
  View::PaintChildren(canvas);
}

int OmniboxPopupContentsView::CalculatePopupHeight() {
  DCHECK_GE(static_cast<size_t>(child_count()), model_->result().size());
  int popup_height = 0;
  for (size_t i = 0; i < model_->result().size(); ++i)
    popup_height += child_at(i)->GetPreferredSize().height();
  return popup_height;
}

OmniboxResultView* OmniboxPopupContentsView::CreateResultView(
    OmniboxResultViewModel* model,
    int model_index,
    const gfx::Font& font) {
  return new OmniboxResultView(model, model_index, font);
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, views::View overrides, protected:

void OmniboxPopupContentsView::OnPaint(gfx::Canvas* canvas) {
  gfx::Path path;
  MakeContentsPath(&path, GetContentsBounds());
  canvas->Save();
  canvas->sk_canvas()->clipPath(path,
                                SkRegion::kIntersect_Op,
                                true /* doAntialias */);
  PaintResultViews(canvas);

  // We want the contents background to be slightly transparent so we can see
  // the blurry glass effect on DWM systems behind. We do this _after_ we paint
  // the children since they paint text, and GDI will reset this alpha data if
  // we paint text after this call.
  MakeCanvasTransparent(canvas);
  canvas->Restore();

  // Now we paint the border, so it will be alpha-blended atop the contents.
  // This looks slightly better in the corners than drawing the contents atop
  // the border.
  OnPaintBorder(canvas);
}

void OmniboxPopupContentsView::PaintChildren(gfx::Canvas* canvas) {
  // We paint our children inside OnPaint().
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, private:

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

  SkScalar radius = SkIntToScalar(views::BubbleBorder::GetCornerRadius());
  path->addRoundRect(rect, radius, radius);
}

void OmniboxPopupContentsView::UpdateBlurRegion() {
#if defined(OS_WIN) && !defined(USE_AURA)
  // We only support background blurring on Vista with Aero-Glass enabled.
  if (!ui::win::IsAeroGlassEnabled() || !GetWidget())
    return;

  // Provide a blurred background effect within the contents region of the
  // popup.
  DWM_BLURBEHIND bb = {0};
  bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
  bb.fEnable = true;

  // Translate the contents rect into widget coordinates, since that's what
  // DwmEnableBlurBehindWindow expects a region in.
  gfx::Rect contents_rect = ConvertRectToWidget(GetContentsBounds());
  gfx::Path contents_path;
  MakeContentsPath(&contents_path, contents_rect);
  base::win::ScopedGDIObject<HRGN> popup_region;
  popup_region.Set(contents_path.CreateNativeRegion());
  bb.hRgnBlur = popup_region.Get();
  DwmEnableBlurBehindWindow(GetWidget()->GetNativeView(), &bb);
#endif
}

void OmniboxPopupContentsView::MakeCanvasTransparent(gfx::Canvas* canvas) {
  // Allow the window blur effect to show through the popup background.
  SkAlpha alpha = GetThemeProvider()->ShouldUseNativeFrame() ?
      kGlassPopupAlpha : kOpaquePopupAlpha;
  canvas->DrawColor(SkColorSetA(
      result_view_at(0)->GetColor(OmniboxResultView::NORMAL,
          OmniboxResultView::BACKGROUND), alpha), SkXfermode::kDstIn_Mode);
}

void OmniboxPopupContentsView::OpenIndex(size_t index,
                                         WindowOpenDisposition disposition) {
  if (!HasMatchAt(index))
    return;

  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  AutocompleteMatch match = model_->result().match_at(index);
  omnibox_view_->OpenMatch(match, disposition, GURL(), index);
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
    if (child->HitTestPoint(point_in_child_coords))
      return i;
  }
  return OmniboxPopupModel::kNoMatch;
}

gfx::Rect OmniboxPopupContentsView::CalculateTargetBounds(int h) {
  gfx::Rect location_bar_bounds(location_bar_->GetContentsBounds());
  const views::Border* border = location_bar_->border();
  if (border) {
    // Adjust for the border so that the bubble and location bar borders are
    // aligned.
    gfx::Insets insets = border->GetInsets();
    location_bar_bounds.Inset(insets.left(), 0, insets.right(), 0);
  } else {
    // The normal location bar is drawn using a background graphic that includes
    // the border, so we inset by enough to make the edges line up, and the
    // bubble appear at the same height as the Star bubble.
    location_bar_bounds.Inset(LocationBarView::kNormalHorizontalEdgeThickness,
                              0);
  }
  gfx::Point location_bar_origin(location_bar_bounds.origin());
  views::View::ConvertPointToScreen(location_bar_, &location_bar_origin);
  location_bar_bounds.set_origin(location_bar_origin);
  return bubble_border_->GetBounds(
      location_bar_bounds, gfx::Size(location_bar_bounds.width(), h));
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
  OpenIndex(index, disposition);
}

OmniboxResultView* OmniboxPopupContentsView::result_view_at(size_t i) {
  return static_cast<OmniboxResultView*>(child_at(static_cast<int>(i)));
}
