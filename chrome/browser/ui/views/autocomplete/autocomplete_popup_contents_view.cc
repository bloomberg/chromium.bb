// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_contents_view.h"

#if defined(OS_WIN)
#include <commctrl.h>
#include <dwmapi.h>
#include <objidl.h>
#endif

#include "base/compiler_specific.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/autocomplete/autocomplete_result_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/autocomplete/touch_autocomplete_popup_contents_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/path.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"
#include "unicode/ubidi.h"

#if defined(OS_WIN)
#include "base/win/scoped_gdi_object.h"
#if !defined(USE_AURA)
#include "ui/views/widget/native_widget_win.h"
#endif
#endif
#if defined(USE_ASH)
#include "ash/wm/window_animations.h"
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

class AutocompletePopupContentsView::AutocompletePopupWidget
    : public views::Widget,
      public base::SupportsWeakPtr<AutocompletePopupWidget> {
 public:
  AutocompletePopupWidget() {}
  virtual ~AutocompletePopupWidget() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupWidget);
};

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, public:

AutocompletePopupContentsView*
    AutocompletePopupContentsView::CreateForEnvironment(
        const gfx::Font& font,
        OmniboxView* omnibox_view,
        AutocompleteEditModel* edit_model,
        views::View* location_bar) {
  AutocompletePopupContentsView* view = NULL;
  if (ui::GetDisplayLayout() == ui::LAYOUT_TOUCH) {
    view = new TouchAutocompletePopupContentsView(
        font, omnibox_view, edit_model, location_bar);
  } else {
    view = new AutocompletePopupContentsView(
        font, omnibox_view, edit_model, location_bar);
  }

  view->Init();
  return view;
}

AutocompletePopupContentsView::AutocompletePopupContentsView(
    const gfx::Font& font,
    OmniboxView* omnibox_view,
    AutocompleteEditModel* edit_model,
    views::View* location_bar)
    : model_(new AutocompletePopupModel(this, edit_model)),
      omnibox_view_(omnibox_view),
      profile_(edit_model->profile()),
      location_bar_(location_bar),
      result_font_(font.DeriveFont(kEditFontAdjust)),
      result_bold_font_(result_font_.DeriveFont(0, gfx::Font::BOLD)),
      ignore_mouse_drag_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(size_animation_(this)) {
  // The following little dance is required because set_border() requires a
  // pointer to a non-const object.
  views::BubbleBorder* bubble_border =
      new views::BubbleBorder(views::BubbleBorder::NONE,
                              views::BubbleBorder::NO_SHADOW);
  bubble_border_ = bubble_border;
  set_border(bubble_border);
  // The contents is owned by the LocationBarView.
  set_owned_by_client();
}

void AutocompletePopupContentsView::Init() {
  // This can't be done in the constructor as at that point we aren't
  // necessarily our final class yet, and we may have subclasses
  // overriding CreateResultView.
  for (size_t i = 0; i < AutocompleteResult::kMaxMatches; ++i) {
    AutocompleteResultView* result_view =
        CreateResultView(this, i, result_font_, result_bold_font_);
    result_view->SetVisible(false);
    AddChildViewAt(result_view, static_cast<int>(i));
  }
}

AutocompletePopupContentsView::~AutocompletePopupContentsView() {
  // We don't need to do anything with |popup_| here.  The OS either has already
  // closed the window, in which case it's been deleted, or it will soon, in
  // which case there's nothing we need to do.
}

gfx::Rect AutocompletePopupContentsView::GetPopupBounds() const {
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

void AutocompletePopupContentsView::LayoutChildren() {
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
// AutocompletePopupContentsView, AutocompletePopupView overrides:

bool AutocompletePopupContentsView::IsOpen() const {
  return (popup_ != NULL);
}

void AutocompletePopupContentsView::InvalidateLine(size_t line) {
  AutocompleteResultView* result = static_cast<AutocompleteResultView*>(
      child_at(static_cast<int>(line)));
  result->Invalidate();

  if (HasMatchAt(line) && GetMatchAtIndex(line).associated_keyword.get()) {
    result->ShowKeyword(IsSelectedIndex(line) &&
        model_->selected_line_state() == AutocompletePopupModel::KEYWORD);
  }
}

void AutocompletePopupContentsView::UpdatePopupAppearance() {
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
    AutocompleteResultView* view = static_cast<AutocompleteResultView*>(
        child_at(i));
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
    params.parent_widget = location_bar_->GetWidget();
    params.bounds = GetPopupBounds();
    popup_->Init(params);
#if defined(USE_ASH)
    ash::SetWindowVisibilityAnimationType(
        popup_->GetNativeView(),
        ash::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
    // No animation for autocomplete popup appearance.  see crbug.com/124104
    ash::SetWindowVisibilityAnimationTransition(
        popup_->GetNativeView(), ash::ANIMATE_HIDE);
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
    start_bounds_ = GetWidget()->GetWindowScreenBounds();
    if (target_bounds_.height() < start_bounds_.height())
      size_animation_.Show();
    else
      start_bounds_ = target_bounds_;
    popup_->SetBounds(GetPopupBounds());
  }

  SchedulePaint();
}

gfx::Rect AutocompletePopupContentsView::GetTargetBounds() {
  return target_bounds_;
}

void AutocompletePopupContentsView::PaintUpdatesNow() {
  // TODO(beng): remove this from the interface.
}

void AutocompletePopupContentsView::OnDragCanceled() {
  ignore_mouse_drag_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, AutocompleteResultViewModel implementation:

bool AutocompletePopupContentsView::IsSelectedIndex(size_t index) const {
  return index == model_->selected_line();
}

bool AutocompletePopupContentsView::IsHoveredIndex(size_t index) const {
  return index == model_->hovered_line();
}

const SkBitmap* AutocompletePopupContentsView::GetIconIfExtensionMatch(
    size_t index) const {
  if (!HasMatchAt(index))
    return NULL;
  return model_->GetIconIfExtensionMatch(GetMatchAtIndex(index));
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, AnimationDelegate implementation:

void AutocompletePopupContentsView::AnimationProgressed(
    const ui::Animation* animation) {
  // We should only be running the animation when the popup is already visible.
  DCHECK(popup_ != NULL);
  popup_->SetBounds(GetPopupBounds());
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, views::View overrides:

void AutocompletePopupContentsView::Layout() {
  UpdateBlurRegion();

  // Size our children to the available content area.
  LayoutChildren();

  // We need to manually schedule a paint here since we are a layered window and
  // won't implicitly require painting until we ask for one.
  SchedulePaint();
}

views::View* AutocompletePopupContentsView::GetEventHandlerForPoint(
    const gfx::Point& point) {
  return this;
}

bool AutocompletePopupContentsView::OnMousePressed(
    const views::MouseEvent& event) {
  ignore_mouse_drag_ = false;  // See comment on |ignore_mouse_drag_| in header.
  if (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) {
    size_t index = GetIndexForPoint(event.location());
    model_->SetHoveredLine(index);
    if (HasMatchAt(index) && event.IsLeftMouseButton())
      model_->SetSelectedLine(index, false, false);
  }
  return true;
}

bool AutocompletePopupContentsView::OnMouseDragged(
    const views::MouseEvent& event) {
  if (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) {
    size_t index = GetIndexForPoint(event.location());
    model_->SetHoveredLine(index);
    if (!ignore_mouse_drag_ && HasMatchAt(index) && event.IsLeftMouseButton())
      model_->SetSelectedLine(index, false, false);
  }
  return true;
}

void AutocompletePopupContentsView::OnMouseReleased(
    const views::MouseEvent& event) {
  if (ignore_mouse_drag_) {
    OnMouseCaptureLost();
    return;
  }

  size_t index = GetIndexForPoint(event.location());
  if (event.IsOnlyMiddleMouseButton())
    OpenIndex(index, NEW_BACKGROUND_TAB);
  else if (event.IsOnlyLeftMouseButton())
    OpenIndex(index, CURRENT_TAB);
}

void AutocompletePopupContentsView::OnMouseCaptureLost() {
  ignore_mouse_drag_ = false;
}

void AutocompletePopupContentsView::OnMouseMoved(
    const views::MouseEvent& event) {
  model_->SetHoveredLine(GetIndexForPoint(event.location()));
}

void AutocompletePopupContentsView::OnMouseEntered(
    const views::MouseEvent& event) {
  model_->SetHoveredLine(GetIndexForPoint(event.location()));
}

void AutocompletePopupContentsView::OnMouseExited(
    const views::MouseEvent& event) {
  model_->SetHoveredLine(AutocompletePopupModel::kNoMatch);
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, protected:

void AutocompletePopupContentsView::PaintResultViews(gfx::Canvas* canvas) {
  canvas->DrawColor(AutocompleteResultView::GetColor(
      AutocompleteResultView::NORMAL, AutocompleteResultView::BACKGROUND));
  View::PaintChildren(canvas);
}

int AutocompletePopupContentsView::CalculatePopupHeight() {
  DCHECK_GE(static_cast<size_t>(child_count()), model_->result().size());
  int popup_height = 0;
  for (size_t i = 0; i < model_->result().size(); ++i)
    popup_height += child_at(i)->GetPreferredSize().height();
  return popup_height;
}

AutocompleteResultView* AutocompletePopupContentsView::CreateResultView(
    AutocompleteResultViewModel* model,
    int model_index,
    const gfx::Font& font,
    const gfx::Font& bold_font) {
  return new AutocompleteResultView(model, model_index, font, bold_font);
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, views::View overrides, protected:

void AutocompletePopupContentsView::OnPaint(gfx::Canvas* canvas) {
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

void AutocompletePopupContentsView::PaintChildren(gfx::Canvas* canvas) {
  // We paint our children inside OnPaint().
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, private:

bool AutocompletePopupContentsView::HasMatchAt(size_t index) const {
  return index < model_->result().size();
}

const AutocompleteMatch& AutocompletePopupContentsView::GetMatchAtIndex(
    size_t index) const {
  return model_->result().match_at(index);
}

void AutocompletePopupContentsView::MakeContentsPath(
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

void AutocompletePopupContentsView::UpdateBlurRegion() {
#if defined(OS_WIN) && !defined(USE_AURA)
  // We only support background blurring on Vista with Aero-Glass enabled.
  if (!views::NativeWidgetWin::IsAeroGlassEnabled() || !GetWidget())
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

void AutocompletePopupContentsView::MakeCanvasTransparent(
    gfx::Canvas* canvas) {
  // Allow the window blur effect to show through the popup background.
  SkAlpha alpha = GetThemeProvider()->ShouldUseNativeFrame() ?
      kGlassPopupAlpha : kOpaquePopupAlpha;
  canvas->DrawColor(SkColorSetA(
      AutocompleteResultView::GetColor(AutocompleteResultView::NORMAL,
      AutocompleteResultView::BACKGROUND), alpha), SkXfermode::kDstIn_Mode);
}

void AutocompletePopupContentsView::OpenIndex(
    size_t index,
    WindowOpenDisposition disposition) {
  if (!HasMatchAt(index))
    return;

  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  AutocompleteMatch match = model_->result().match_at(index);
  omnibox_view_->OpenMatch(match, disposition, GURL(), index);
}

size_t AutocompletePopupContentsView::GetIndexForPoint(
    const gfx::Point& point) {
  if (!HitTest(point))
    return AutocompletePopupModel::kNoMatch;

  int nb_match = model_->result().size();
  DCHECK(nb_match <= child_count());
  for (int i = 0; i < nb_match; ++i) {
    views::View* child = child_at(i);
    gfx::Point point_in_child_coords(point);
    View::ConvertPointToView(this, child, &point_in_child_coords);
    if (child->HitTest(point_in_child_coords))
      return i;
  }
  return AutocompletePopupModel::kNoMatch;
}

gfx::Rect AutocompletePopupContentsView::CalculateTargetBounds(int h) {
  gfx::Rect location_bar_bounds(location_bar_->GetContentsBounds());
  const views::Border* border = location_bar_->border();
  if (border) {
    // Adjust for the border so that the bubble and location bar borders are
    // aligned.
    gfx::Insets insets;
    border->GetInsets(&insets);
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
