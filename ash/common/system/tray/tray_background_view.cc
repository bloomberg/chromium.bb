// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_background_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_event_filter.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/system/tray/system_tray.h"
#include "grit/ash_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/nine_image_painter.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
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

  explicit TrayBackground(TrayBackgroundView* tray_background_view)
      : tray_background_view_(tray_background_view) {}

  ~TrayBackground() override {}

 private:
  WmShelf* GetShelf() const { return tray_background_view_->shelf(); }

  void PaintMaterial(gfx::Canvas* canvas, views::View* view) const {
    SkColor background_color = SK_ColorTRANSPARENT;
    if (GetShelf()->GetBackgroundType() ==
        ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT) {
      background_color = SkColorSetA(kShelfBaseColor,
                                     GetShelfConstant(SHELF_BACKGROUND_ALPHA));
    }

    // TODO(bruthig|tdanderson): The background should be changed using a
    // fade in/out animation.
    SkPaint background_paint;
    background_paint.setFlags(SkPaint::kAntiAlias_Flag);
    background_paint.setColor(background_color);
    canvas->DrawRoundRect(view->GetLocalBounds(), kTrayRoundedBorderRadius,
                          background_paint);

    if (tray_background_view_->draw_background_as_active()) {
      SkPaint highlight_paint;
      highlight_paint.setFlags(SkPaint::kAntiAlias_Flag);
      highlight_paint.setColor(kShelfButtonActivatedHighlightColor);
      canvas->DrawRoundRect(view->GetLocalBounds(), kTrayRoundedBorderRadius,
                            highlight_paint);
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
    if (tray_background_view_->draw_background_as_active())
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

gfx::Size TrayBackgroundView::TrayContainer::GetPreferredSize() const {
  if (size_.IsEmpty())
    return views::View::GetPreferredSize();
  return size_;
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
  // Adjust the size of status tray dark background by adding additional
  // empty border.
  views::BoxLayout::Orientation orientation =
      IsHorizontalAlignment(alignment_) ? views::BoxLayout::kHorizontal
                                        : views::BoxLayout::kVertical;

  if (!ash::MaterialDesignController::IsShelfMaterial()) {
    // Additional padding used to adjust the user-visible size of status tray
    // dark background.
    const int padding = 3;
    SetBorder(
        views::Border::CreateEmptyBorder(padding, padding, padding, padding));
  }

  views::BoxLayout* layout = new views::BoxLayout(orientation, 0, 0, 0);
  layout->SetDefaultFlex(1);
  views::View::SetLayoutManager(layout);
  PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// TrayBackgroundView

TrayBackgroundView::TrayBackgroundView(WmShelf* wm_shelf)
    : wm_shelf_(wm_shelf),
      tray_container_(NULL),
      shelf_alignment_(SHELF_ALIGNMENT_BOTTOM),
      background_(NULL),
      draw_background_as_active_(false),
      widget_observer_(new TrayWidgetObserver(this)) {
  DCHECK(wm_shelf_);
  set_notify_enter_exit_on_child(true);

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
}

const char* TrayBackgroundView::GetClassName() const {
  return kViewClassName;
}

void TrayBackgroundView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void TrayBackgroundView::GetAccessibleState(ui::AXViewState* state) {
  ActionableView::GetAccessibleState(state);
  state->name = GetAccessibleNameForTray();
}

void TrayBackgroundView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  // Return focus to the login view. See crbug.com/120500.
  views::View* v = GetNextFocusableView();
  if (v)
    v->AboutToRequestFocusFromTabTraversal(reverse);
}

bool TrayBackgroundView::PerformAction(const ui::Event& event) {
  return false;
}

gfx::Rect TrayBackgroundView::GetFocusBounds() {
  // The tray itself expands to the right and bottom edge of the screen to make
  // sure clicking on the edges brings up the popup. However, the focus border
  // should be only around the container.
  return GetContentsBounds();
}

void TrayBackgroundView::OnGestureEvent(ui::GestureEvent* event) {
  if (switches::IsTouchFeedbackEnabled()) {
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
      SetDrawBackgroundAsActive(true);
    } else if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
               event->type() == ui::ET_GESTURE_TAP_CANCEL) {
      SetDrawBackgroundAsActive(false);
    }
  }
  ActionableView::OnGestureEvent(event);
}

void TrayBackgroundView::UpdateBackground(int alpha) {
  // The animator should never fire when the alternate shelf layout is used.
  if (!background_ || draw_background_as_active_)
    return;
  SchedulePaint();
}

void TrayBackgroundView::SetContents(views::View* contents) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(contents);
}
void TrayBackgroundView::SetContentsBackground() {
  background_ = new TrayBackground(this);
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

gfx::Rect TrayBackgroundView::GetBubbleAnchorRect(
    views::Widget* anchor_widget,
    TrayBubbleView::AnchorType anchor_type,
    TrayBubbleView::AnchorAlignment anchor_alignment) const {
  gfx::Rect rect;
  if (anchor_widget && anchor_widget->IsVisible()) {
    rect = anchor_widget->GetWindowBoundsInScreen();
    if (anchor_type == TrayBubbleView::ANCHOR_TYPE_TRAY) {
      if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
        bool rtl = base::i18n::IsRTL();
        rect.Inset(rtl ? kBubblePaddingHorizontalSide : 0,
                   kBubblePaddingHorizontalBottom,
                   rtl ? 0 : kBubblePaddingHorizontalSide, 0);
      } else if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_LEFT) {
        rect.Inset(0, 0, kBubblePaddingVerticalSide + 4,
                   kBubblePaddingVerticalBottom);
      } else if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT) {
        rect.Inset(kBubblePaddingVerticalSide, 0, 0,
                   kBubblePaddingVerticalBottom);
      } else {
        // TODO(bruthig) May need to handle other ANCHOR_ALIGNMENT_ values.
        // ie. ANCHOR_ALIGNMENT_TOP
        DCHECK(false) << "Unhandled anchor alignment.";
      }
    } else if (anchor_type == TrayBubbleView::ANCHOR_TYPE_BUBBLE) {
      // Invert the offsets to align with the bubble below.
      // Note that with the alternate shelf layout the tips are not shown and
      // the offsets for left and right alignment do not need to be applied.
      int vertical_alignment = 0;
      int horizontal_alignment = kBubblePaddingVerticalBottom;
      if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_LEFT)
        rect.Inset(vertical_alignment, 0, 0, horizontal_alignment);
      else if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT)
        rect.Inset(0, 0, vertical_alignment, horizontal_alignment);
    } else {
      DCHECK(false) << "Unhandled anchor type.";
    }
  } else {
    WmWindow* target_root = anchor_widget
                                ? WmLookup::Get()
                                      ->GetWindowForWidget(anchor_widget)
                                      ->GetRootWindow()
                                : WmShell::Get()->GetPrimaryRootWindow();
    rect = target_root->GetBounds();
    if (anchor_type == TrayBubbleView::ANCHOR_TYPE_TRAY) {
      if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
        rect = gfx::Rect(
            base::i18n::IsRTL()
                ? kPaddingFromRightEdgeOfScreenBottomAlignment
                : rect.width() - kPaddingFromRightEdgeOfScreenBottomAlignment,
            rect.height() - kPaddingFromBottomOfScreenBottomAlignment, 0, 0);
        rect = target_root->ConvertRectToScreen(rect);
      } else if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_LEFT) {
        rect = gfx::Rect(
            kPaddingFromRightEdgeOfScreenBottomAlignment,
            rect.height() - kPaddingFromBottomOfScreenBottomAlignment, 1, 1);
        rect = target_root->ConvertRectToScreen(rect);
      } else if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT) {
        rect = gfx::Rect(
            rect.width() - kPaddingFromRightEdgeOfScreenBottomAlignment,
            rect.height() - kPaddingFromBottomOfScreenBottomAlignment, 1, 1);
        rect = target_root->ConvertRectToScreen(rect);
      } else {
        // TODO(bruthig) May need to handle other ANCHOR_ALIGNMENT_ values.
        // ie. ANCHOR_ALIGNMENT_TOP
        DCHECK(false) << "Unhandled anchor alignment.";
      }
    } else {
      rect = gfx::Rect(
          base::i18n::IsRTL()
              ? kPaddingFromRightEdgeOfScreenBottomAlignment
              : rect.width() - kPaddingFromRightEdgeOfScreenBottomAlignment,
          rect.height() - kPaddingFromBottomOfScreenBottomAlignment, 0, 0);
    }
  }
  return rect;
}

TrayBubbleView::AnchorAlignment TrayBackgroundView::GetAnchorAlignment() const {
  if (shelf_alignment_ == SHELF_ALIGNMENT_LEFT)
    return TrayBubbleView::ANCHOR_ALIGNMENT_LEFT;
  if (shelf_alignment_ == SHELF_ALIGNMENT_RIGHT)
    return TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT;
  return TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM;
}

void TrayBackgroundView::SetDrawBackgroundAsActive(bool visible) {
  if (draw_background_as_active_ == visible)
    return;
  draw_background_as_active_ = visible;
  if (!background_)
    return;
  SchedulePaint();
}

void TrayBackgroundView::UpdateBubbleViewArrow(
    views::TrayBubbleView* bubble_view) {
  // Nothing to do here.
}

}  // namespace ash
