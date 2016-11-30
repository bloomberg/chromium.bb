// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_background_view.h"

#include <algorithm>

#include "ash/common/ash_constants.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_event_filter.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "grit/ash_resources.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/nine_image_painter.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/wm/core/window_animations.h"

namespace {

const int kAnimationDurationForPopupMs = 200;

// Duration of opacity animation for visibility changes.
const int kAnimationDurationForVisibilityMs = 250;

// When becoming visible delay the animation so that StatusAreaWidgetDelegate
// can animate sibling views out of the position to be occuped by the
// TrayBackgroundView.
const int kShowAnimationDelayMs = 100;

// Additional padding used to adjust the user-visible size of status tray
// and overview button dark background.
const int kBackgroundAdjustPadding = 3;

// Switches left and right insets if RTL mode is active.
void MirrorInsetsIfNecessary(gfx::Insets* insets) {
  if (base::i18n::IsRTL()) {
    insets->Set(insets->top(), insets->right(), insets->bottom(),
                insets->left());
  }
}

// Returns background insets relative to the contents bounds of the view and
// mirrored if RTL mode is active.
gfx::Insets GetMirroredBackgroundInsets(ash::ShelfAlignment shelf_alignment) {
  gfx::Insets insets;
  if (IsHorizontalAlignment(shelf_alignment)) {
    insets.Set(0, ash::kHitRegionPadding, 0,
               ash::kHitRegionPadding + ash::kSeparatorWidth);
  } else {
    insets.Set(ash::kHitRegionPadding, 0,
               ash::kHitRegionPadding + ash::kSeparatorWidth, 0);
  }
  MirrorInsetsIfNecessary(&insets);
  return insets;
}

}  // namespace

using views::TrayBubbleView;

namespace ash {

// static
const char TrayBackgroundView::kViewClassName[] = "tray/TrayBackgroundView";

// Used to track when the anchor widget changes position on screen so that the
// bubble position can be updated.
class TrayBackgroundView::TrayWidgetObserver : public views::WidgetObserver {
 public:
  explicit TrayWidgetObserver(TrayBackgroundView* host) : host_(host) {}

  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    host_->AnchorUpdated();
  }

  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    host_->AnchorUpdated();
  }

 private:
  TrayBackgroundView* host_;

  DISALLOW_COPY_AND_ASSIGN(TrayWidgetObserver);
};

class TrayBackground : public views::Background {
 public:
  const static int kImageTypeDefault = 0;
  const static int kImageTypeOnBlack = 1;
  const static int kImageTypePressed = 2;
  const static int kNumStates = 3;

  const static int kImageHorizontal = 0;
  const static int kImageVertical = 1;
  const static int kNumOrientations = 2;

  TrayBackground(TrayBackgroundView* tray_background_view, bool draws_active)
      : tray_background_view_(tray_background_view),
        draws_active_(draws_active),
        alpha_(0) {}

  ~TrayBackground() override {}

  void set_alpha(int alpha) { alpha_ = alpha; }

 private:
  WmShelf* GetShelf() const { return tray_background_view_->shelf(); }

  void PaintMaterial(gfx::Canvas* canvas, views::View* view) const {
    SkPaint background_paint;
    background_paint.setFlags(SkPaint::kAntiAlias_Flag);
    background_paint.setColor(SkColorSetA(kShelfBaseColor, alpha_));
    gfx::Insets insets =
        GetMirroredBackgroundInsets(GetShelf()->GetAlignment());
    gfx::Rect bounds = view->GetLocalBounds();
    bounds.Inset(insets);
    canvas->DrawRoundRect(bounds, kTrayRoundedBorderRadius, background_paint);

    if (draws_active_ && tray_background_view_->is_active()) {
      SkPaint highlight_paint;
      highlight_paint.setFlags(SkPaint::kAntiAlias_Flag);
      highlight_paint.setColor(kShelfButtonActivatedHighlightColor);
      canvas->DrawRoundRect(bounds, kTrayRoundedBorderRadius, highlight_paint);
    }
  }

  void PaintNonMaterial(gfx::Canvas* canvas, views::View* view) const {
    const int kGridSizeForPainter = 9;
    const int kImages[kNumOrientations][kNumStates][kGridSizeForPainter] = {
        {
            // Horizontal
            IMAGE_GRID_HORIZONTAL(IDR_AURA_TRAY_BG_HORIZ),
            IMAGE_GRID_HORIZONTAL(IDR_AURA_TRAY_BG_HORIZ_ONBLACK),
            IMAGE_GRID_HORIZONTAL(IDR_AURA_TRAY_BG_HORIZ_PRESSED),
        },
        {
            // Vertical
            IMAGE_GRID_VERTICAL(IDR_AURA_TRAY_BG_VERTICAL),
            IMAGE_GRID_VERTICAL(IDR_AURA_TRAY_BG_VERTICAL_ONBLACK),
            IMAGE_GRID_VERTICAL(IDR_AURA_TRAY_BG_VERTICAL_PRESSED),
        }};

    WmShelf* shelf = GetShelf();
    const int orientation = IsHorizontalAlignment(shelf->GetAlignment())
                                ? kImageHorizontal
                                : kImageVertical;

    int state = kImageTypeDefault;
    if (draws_active_ && tray_background_view_->is_active())
      state = kImageTypePressed;
    else if (shelf->IsDimmed())
      state = kImageTypeOnBlack;
    else
      state = kImageTypeDefault;

    ui::CreateNineImagePainter(kImages[orientation][state])
        ->Paint(canvas, view->GetLocalBounds());
  }

  // Overridden from views::Background.
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    if (MaterialDesignController::IsShelfMaterial())
      PaintMaterial(canvas, view);
    else
      PaintNonMaterial(canvas, view);
  }

  // Reference to the TrayBackgroundView for which this is a background.
  TrayBackgroundView* tray_background_view_;

  // Determines whether we should draw an active background for the view when it
  // is active. This is used in non-MD mode. In material design mode, an active
  // ink drop ripple would indicate if the view is active or not.
  // TODO(mohsen): This is used only in non-MD version. Remove when non-MD code
  // is removed (see https://crbug.com/614453).
  bool draws_active_;

  int alpha_;

  DISALLOW_COPY_AND_ASSIGN(TrayBackground);
};

TrayBackgroundView::TrayContainer::TrayContainer(ShelfAlignment alignment)
    : alignment_(alignment) {
  UpdateLayout();
}

void TrayBackgroundView::TrayContainer::SetAlignment(ShelfAlignment alignment) {
  if (alignment_ == alignment)
    return;
  alignment_ = alignment;
  UpdateLayout();
}

void TrayBackgroundView::TrayContainer::SetMargin(int main_axis_margin,
                                                  int cross_axis_margin) {
  main_axis_margin_ = main_axis_margin;
  cross_axis_margin_ = cross_axis_margin;
  UpdateLayout();
}

void TrayBackgroundView::TrayContainer::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void TrayBackgroundView::TrayContainer::ChildVisibilityChanged(View* child) {
  PreferredSizeChanged();
}

void TrayBackgroundView::TrayContainer::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.parent == this)
    PreferredSizeChanged();
}

void TrayBackgroundView::TrayContainer::UpdateLayout() {
  bool is_horizontal = IsHorizontalAlignment(alignment_);

  // Adjust the size of status tray dark background by adding additional
  // empty border.
  views::BoxLayout::Orientation orientation =
      is_horizontal ? views::BoxLayout::kHorizontal
                    : views::BoxLayout::kVertical;

  if (ash::MaterialDesignController::IsShelfMaterial()) {
    const int hit_region_with_separator = kHitRegionPadding + kSeparatorWidth;
    gfx::Insets insets(
        is_horizontal
            ? gfx::Insets(0, kHitRegionPadding, 0, hit_region_with_separator)
            : gfx::Insets(kHitRegionPadding, 0, hit_region_with_separator, 0));
    MirrorInsetsIfNecessary(&insets);
    SetBorder(views::CreateEmptyBorder(insets));
  } else {
    SetBorder(views::CreateEmptyBorder(gfx::Insets(kBackgroundAdjustPadding)));
  }

  int horizontal_margin = main_axis_margin_;
  int vertical_margin = cross_axis_margin_;
  if (!is_horizontal)
    std::swap(horizontal_margin, vertical_margin);
  views::BoxLayout* layout =
      new views::BoxLayout(orientation, horizontal_margin, vertical_margin, 0);

  if (!ash::MaterialDesignController::IsShelfMaterial())
    layout->SetDefaultFlex(1);
  layout->set_minimum_cross_axis_size(kTrayItemSize);
  views::View::SetLayoutManager(layout);

  PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// TrayBackgroundView

TrayBackgroundView::TrayBackgroundView(WmShelf* wm_shelf)
    // Note the ink drop style is ignored.
    : ActionableView(nullptr, TrayPopupInkDropStyle::FILL_BOUNDS),
      wm_shelf_(wm_shelf),
      tray_container_(NULL),
      shelf_alignment_(SHELF_ALIGNMENT_BOTTOM),
      background_(NULL),
      is_active_(false),
      is_separator_visible_(false),
      widget_observer_(new TrayWidgetObserver(this)) {
  DCHECK(wm_shelf_);
  set_notify_enter_exit_on_child(true);
  set_ink_drop_base_color(kShelfInkDropBaseColor);
  set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);

  tray_container_ = new TrayContainer(shelf_alignment_);
  SetContents(tray_container_);
  tray_event_filter_.reset(new TrayEventFilter);

  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
  // Start the tray items not visible, because visibility changes are animated.
  views::View::SetVisible(false);
}

TrayBackgroundView::~TrayBackgroundView() {
  if (GetWidget())
    GetWidget()->RemoveObserver(widget_observer_.get());
  StopObservingImplicitAnimations();
}

void TrayBackgroundView::Initialize() {
  GetWidget()->AddObserver(widget_observer_.get());
}

// static
void TrayBackgroundView::InitializeBubbleAnimations(
    views::Widget* bubble_widget) {
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(bubble_widget);
  window->SetVisibilityAnimationType(
      ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  window->SetVisibilityAnimationTransition(::wm::ANIMATE_HIDE);
  window->SetVisibilityAnimationDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationForPopupMs));
}

void TrayBackgroundView::SetVisible(bool visible) {
  if (visible == layer()->GetTargetVisibility())
    return;

  if (visible) {
    // The alignment of the shelf can change while the TrayBackgroundView is
    // hidden. Reset the offscreen transform so that the animation to becoming
    // visible reflects the current layout.
    HideTransformation();
    // SetVisible(false) is defered until the animation for hiding is done.
    // Otherwise the view is immediately hidden and the animation does not
    // render.
    views::View::SetVisible(true);
    // If SetVisible(true) is called while animating to not visible, then
    // views::View::SetVisible(true) is a no-op. When the previous animation
    // ends layer->SetVisible(false) is called. To prevent this
    // layer->SetVisible(true) immediately interrupts the animation of this
    // property, and keeps the layer visible.
    layer()->SetVisible(true);
  }

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationForVisibilityMs));
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  if (visible) {
    animation.SetTweenType(gfx::Tween::EASE_OUT);
    // Show is delayed so as to allow time for other children of
    // StatusAreaWidget to begin animating to their new positions.
    layer()->GetAnimator()->SchedulePauseForProperties(
        base::TimeDelta::FromMilliseconds(kShowAnimationDelayMs),
        ui::LayerAnimationElement::OPACITY |
            ui::LayerAnimationElement::TRANSFORM);
    layer()->SetOpacity(1.0f);
    gfx::Transform transform;
    transform.Translate(0.0f, 0.0f);
    layer()->SetTransform(transform);
  } else {
    // Listen only to the hide animation. As we cannot turn off visibility
    // until the animation is over.
    animation.AddObserver(this);
    animation.SetTweenType(gfx::Tween::EASE_IN);
    layer()->SetOpacity(0.0f);
    layer()->SetVisible(false);
    HideTransformation();
  }
  wm_shelf_->GetStatusAreaWidget()->OnTrayVisibilityChanged(this);
}

const char* TrayBackgroundView::GetClassName() const {
  return kViewClassName;
}

void TrayBackgroundView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void TrayBackgroundView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);
  node_data->SetName(GetAccessibleNameForTray());
}

void TrayBackgroundView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  // Return focus to the login view. See crbug.com/120500.
  views::View* v = GetNextFocusableView();
  if (v)
    v->AboutToRequestFocusFromTabTraversal(reverse);
}

std::unique_ptr<views::InkDropRipple> TrayBackgroundView::CreateInkDropRipple()
    const {
  return base::MakeUnique<views::FloodFillInkDropRipple>(
      size(), GetBackgroundInsets(), GetInkDropCenterBasedOnLastEvent(),
      GetInkDropBaseColor(), ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropHighlight>
TrayBackgroundView::CreateInkDropHighlight() const {
  gfx::Rect bounds = GetBackgroundBounds();
  // Currently, we don't handle view resize. To compensate for that, enlarge the
  // bounds by two tray icons so that the hightlight looks good even if two more
  // icons are added when it is visible. Note that ink drop mask handles resize
  // correctly, so the extra highlight would be clipped.
  // TODO(mohsen): Remove this extra size when resize is handled properly (see
  // https://crbug.com/669253).
  const int icon_size =
      kTrayIconSize + 2 * GetTrayConstant(TRAY_IMAGE_ITEM_PADDING);
  bounds.set_width(bounds.width() + 2 * icon_size);
  bounds.set_height(bounds.height() + 2 * icon_size);
  std::unique_ptr<views::InkDropHighlight> highlight(
      new views::InkDropHighlight(bounds.size(), 0,
                                  gfx::RectF(bounds).CenterPoint(),
                                  GetInkDropBaseColor()));
  highlight->set_visible_opacity(kTrayPopupInkDropHighlightOpacity);
  return highlight;
}

void TrayBackgroundView::OnGestureEvent(ui::GestureEvent* event) {
  // If there is no ink drop, show "touch feedback".
  // TODO(mohsen): This is used only in non-MD version. Remove when non-MD code
  // is removed (see https://crbug.com/614453).
  if (ink_drop_mode() == InkDropMode::OFF) {
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
      SetIsActive(true);
    } else if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
               event->type() == ui::ET_GESTURE_TAP_CANCEL) {
      SetIsActive(false);
    }
  }
  ActionableView::OnGestureEvent(event);
}

void TrayBackgroundView::SetContents(views::View* contents) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(contents);
}

void TrayBackgroundView::SetContentsBackground(bool draws_active) {
  background_ = new TrayBackground(this, draws_active);
  tray_container_->set_background(background_);
}

void TrayBackgroundView::SetShelfAlignment(ShelfAlignment alignment) {
  shelf_alignment_ = alignment;
  tray_container_->SetAlignment(alignment);
}

void TrayBackgroundView::OnImplicitAnimationsCompleted() {
  // If there is another animation in the queue, the reverse animation was
  // triggered before the completion of animating to invisible. Do not turn off
  // the visibility so that the next animation may render. The value of
  // layer()->GetTargetVisibility() can be incorrect if the hide animation was
  // aborted to schedule an animation to become visible. As the new animation
  // is not yet added to the queue. crbug.com/374236
  if (layer()->GetAnimator()->is_animating() || layer()->GetTargetVisibility())
    return;
  views::View::SetVisible(false);
}

bool TrayBackgroundView::RequiresNotificationWhenAnimatorDestroyed() const {
  // This is needed so that OnImplicitAnimationsCompleted() is called even upon
  // destruction of the animator. This can occure when parallel animations
  // caused by ScreenRotationAnimator end before the animations of
  // TrayBackgroundView. This allows for a proper update to the visual state of
  // the view. (crbug.com/476667)
  return true;
}

void TrayBackgroundView::HideTransformation() {
  gfx::Transform transform;
  if (IsHorizontalAlignment(shelf_alignment_))
    transform.Translate(width(), 0.0f);
  else
    transform.Translate(0.0f, height());
  layer()->SetTransform(transform);
}

TrayBubbleView::AnchorAlignment TrayBackgroundView::GetAnchorAlignment() const {
  if (shelf_alignment_ == SHELF_ALIGNMENT_LEFT)
    return TrayBubbleView::ANCHOR_ALIGNMENT_LEFT;
  if (shelf_alignment_ == SHELF_ALIGNMENT_RIGHT)
    return TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT;
  return TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM;
}

void TrayBackgroundView::SetIsActive(bool is_active) {
  if (is_active_ == is_active)
    return;
  is_active_ = is_active;
  AnimateInkDrop(is_active_ ? views::InkDropState::ACTIVATED
                            : views::InkDropState::DEACTIVATED,
                 nullptr);
  if (!background_)
    return;
  // TODO(mohsen): This is needed for non-MD version. Remove when non-MD code is
  // removed (see https://crbug.com/614453).
  SchedulePaint();
}

void TrayBackgroundView::UpdateBubbleViewArrow(
    views::TrayBubbleView* bubble_view) {
  // Nothing to do here.
}

void TrayBackgroundView::UpdateShelfItemBackground(int alpha) {
  if (background_) {
    background_->set_alpha(alpha);
    SchedulePaint();
  }
}

void TrayBackgroundView::SetSeparatorVisibility(bool is_shown) {
  is_separator_visible_ = is_shown;
  SchedulePaint();
}

views::View* TrayBackgroundView::GetBubbleAnchor() const {
  return tray_container_;
}

gfx::Insets TrayBackgroundView::GetBubbleAnchorInsets() const {
  gfx::Insets anchor_insets = GetBubbleAnchor()->GetInsets();
  gfx::Insets tray_bg_insets = GetInsets();
  // TODO(estade): for reasons I don't understand, BubbleBorder distances the
  // bubble by the arrow's "interior" thickness even when the paint type is
  // PAINT_NONE.
  const int kBigShadowArrowInteriorThickness = 9;
  if (GetAnchorAlignment() == TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
    return gfx::Insets(kBigShadowArrowInteriorThickness - tray_bg_insets.top(),
                       anchor_insets.left(), -tray_bg_insets.bottom(),
                       anchor_insets.right());
  } else {
    return gfx::Insets(
        anchor_insets.top(),
        kBigShadowArrowInteriorThickness - tray_bg_insets.left(),
        anchor_insets.bottom(),
        kBigShadowArrowInteriorThickness - tray_bg_insets.right());
  }
}

std::unique_ptr<views::InkDropMask> TrayBackgroundView::CreateInkDropMask()
    const {
  return base::MakeUnique<views::RoundRectInkDropMask>(
      size(), GetBackgroundInsets(), kTrayRoundedBorderRadius);
}

bool TrayBackgroundView::ShouldEnterPushedState(const ui::Event& event) {
  if (is_active_)
    return false;

  return ActionableView::ShouldEnterPushedState(event);
}

bool TrayBackgroundView::PerformAction(const ui::Event& event) {
  return false;
}

void TrayBackgroundView::HandlePerformActionResult(bool action_performed,
                                                   const ui::Event& event) {
  // When an action is performed, ink drop ripple is handled in SetIsActive().
  if (action_performed)
    return;
  ActionableView::HandlePerformActionResult(action_performed, event);
}

gfx::Rect TrayBackgroundView::GetFocusBounds() {
  // The tray itself expands to the right and bottom edge of the screen to make
  // sure clicking on the edges brings up the popup. However, the focus border
  // should be only around the container.
  return GetContentsBounds();
}

void TrayBackgroundView::OnPaintFocus(gfx::Canvas* canvas) {
  gfx::RectF paint_bounds(GetFocusBounds());
  paint_bounds.Inset(gfx::Insets(2, -2));
  canvas->DrawSolidFocusRect(paint_bounds, kFocusBorderColor,
                             kFocusBorderThickness);
}

void TrayBackgroundView::OnPaint(gfx::Canvas* canvas) {
  ActionableView::OnPaint(canvas);
  if (!MaterialDesignController::IsShelfMaterial() ||
      shelf()->GetBackgroundType() ==
          ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT ||
      !is_separator_visible_) {
    return;
  }
  //  In the given |canvas|, for a horizontal shelf draw a separator line to the
  //  right or left of the TrayBackgroundView when the system is LTR or RTL
  //  aligned, respectively. For a vertical shelf draw the separator line
  //  underneath the items instead.
  const bool horizontal_shelf = IsHorizontalAlignment(shelf_alignment_);
  const gfx::Rect local_bounds = GetLocalBounds();
  const int height = kTrayItemSize;
  const int x =
      horizontal_shelf
          ? (base::i18n::IsRTL() ? 0 : (local_bounds.width() - kSeparatorWidth))
          : (local_bounds.height() - kSeparatorWidth);
  const int y = (GetShelfConstant(SHELF_SIZE) - kTrayItemSize) / 2;
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();
  SkPaint paint;
  paint.setColor(kSeparatorColor);
  paint.setAntiAlias(true);

  const gfx::Rect bounds = horizontal_shelf
                               ? gfx::Rect(x, y, kSeparatorWidth, height)
                               : gfx::Rect(y, x, height, kSeparatorWidth);
  gfx::RectF rect(gfx::ScaleRect(gfx::RectF(bounds), scale));
  gfx::PointF line_start =
      horizontal_shelf
          ? (base::i18n::IsRTL() ? rect.origin() : rect.top_right())
          : rect.bottom_left();
  gfx::PointF line_end =
      (horizontal_shelf && base::i18n::IsRTL() ? rect.bottom_left()
                                               : rect.bottom_right());

  canvas->DrawLine(line_start, line_end, paint);
}

gfx::Insets TrayBackgroundView::GetBackgroundInsets() const {
  gfx::Insets insets = GetMirroredBackgroundInsets(shelf_alignment_);

  // |insets| are relative to contents bounds. Change them to be relative to
  // local bounds.
  gfx::Insets local_contents_insets =
      GetLocalBounds().InsetsFrom(GetContentsBounds());
  MirrorInsetsIfNecessary(&local_contents_insets);
  insets += local_contents_insets;

  return insets;
}

gfx::Rect TrayBackgroundView::GetBackgroundBounds() const {
  gfx::Insets insets = GetBackgroundInsets();
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(insets);
  return bounds;
}

}  // namespace ash
