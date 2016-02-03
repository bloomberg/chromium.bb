// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/wm/window_animations.h"
#include "base/command_line.h"
#include "grit/ash_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
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
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace {

const int kTrayBackgroundAlpha = 100;
const int kTrayBackgroundHoverAlpha = 150;
const SkColor kTrayBackgroundPressedColor = SkColorSetRGB(66, 129, 244);

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
  explicit TrayWidgetObserver(TrayBackgroundView* host)
      : host_(host) {
  }

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

  explicit TrayBackground(TrayBackgroundView* tray_background_view) :
      tray_background_view_(tray_background_view) {
    set_alpha(kTrayBackgroundAlpha);
  }

  ~TrayBackground() override {}

  SkColor color() { return color_; }
  void set_color(SkColor color) { color_ = color; }
  void set_alpha(int alpha) { color_ = SkColorSetARGB(alpha, 0, 0, 0); }

 private:
  ShelfWidget* GetShelfWidget() const {
    return RootWindowController::ForWindow(tray_background_view_->
        status_area_widget()->GetNativeWindow())->shelf();
  }

  // Overridden from views::Background.
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    const int kGridSizeForPainter = 9;
    const int kImages[kNumOrientations][kNumStates][kGridSizeForPainter] = {
      { // Horizontal
        IMAGE_GRID_HORIZONTAL(IDR_AURA_TRAY_BG_HORIZ),
        IMAGE_GRID_HORIZONTAL(IDR_AURA_TRAY_BG_HORIZ_ONBLACK),
        IMAGE_GRID_HORIZONTAL(IDR_AURA_TRAY_BG_HORIZ_PRESSED),
      },
      { // Vertical
        IMAGE_GRID_VERTICAL(IDR_AURA_TRAY_BG_VERTICAL),
        IMAGE_GRID_VERTICAL(IDR_AURA_TRAY_BG_VERTICAL_ONBLACK),
        IMAGE_GRID_VERTICAL(IDR_AURA_TRAY_BG_VERTICAL_PRESSED),
      }
    };

    int orientation = kImageHorizontal;
    ShelfWidget* shelf_widget = GetShelfWidget();
    if (shelf_widget &&
        !shelf_widget->shelf_layout_manager()->IsHorizontalAlignment())
      orientation = kImageVertical;

    int state = kImageTypeDefault;
    if (tray_background_view_->draw_background_as_active())
      state = kImageTypePressed;
    else if (shelf_widget && shelf_widget->GetDimsShelf())
      state = kImageTypeOnBlack;
    else
      state = kImageTypeDefault;

    ui::CreateNineImagePainter(kImages[orientation][state])
        ->Paint(canvas, view->GetLocalBounds());
  }

  SkColor color_;
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
  if (alignment_ == SHELF_ALIGNMENT_BOTTOM ||
      alignment_ == SHELF_ALIGNMENT_TOP) {
    SetBorder(views::Border::CreateEmptyBorder(
        kPaddingFromEdgeOfShelf,
        kPaddingFromEdgeOfShelf,
        kPaddingFromEdgeOfShelf,
        kPaddingFromEdgeOfShelf));

    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    layout->SetDefaultFlex(1);
    views::View::SetLayoutManager(layout);
  } else {
    SetBorder(views::Border::CreateEmptyBorder(
        kPaddingFromEdgeOfShelf,
        kPaddingFromEdgeOfShelf,
        kPaddingFromEdgeOfShelf,
        kPaddingFromEdgeOfShelf));

    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
    layout->SetDefaultFlex(1);
    views::View::SetLayoutManager(layout);
  }
  PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// TrayBackgroundView

TrayBackgroundView::TrayBackgroundView(StatusAreaWidget* status_area_widget)
    : status_area_widget_(status_area_widget),
      tray_container_(NULL),
      shelf_alignment_(SHELF_ALIGNMENT_BOTTOM),
      background_(NULL),
      hide_background_animator_(this, 0, kTrayBackgroundAlpha),
      hover_background_animator_(
          this,
          0,
          kTrayBackgroundHoverAlpha - kTrayBackgroundAlpha),
      hovered_(false),
      draw_background_as_active_(false),
      widget_observer_(new TrayWidgetObserver(this)) {
  set_notify_enter_exit_on_child(true);

  // Initially we want to paint the background, but without the hover effect.
  hide_background_animator_.SetPaintsBackground(
      true, BACKGROUND_CHANGE_IMMEDIATE);
  hover_background_animator_.SetPaintsBackground(
      false, BACKGROUND_CHANGE_IMMEDIATE);

  tray_container_ = new TrayContainer(shelf_alignment_);
  SetContents(tray_container_);
  tray_event_filter_.reset(new TrayEventFilter);

  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(false);
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
  SetTrayBorder();
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
  animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      kAnimationDurationForVisibilityMs));
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

void TrayBackgroundView::OnMouseEntered(const ui::MouseEvent& event) {
  hovered_ = true;
}

void TrayBackgroundView::OnMouseExited(const ui::MouseEvent& event) {
  hovered_ = false;
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
    } else if (event->type() ==  ui::ET_GESTURE_SCROLL_BEGIN ||
               event->type() ==  ui::ET_GESTURE_TAP_CANCEL) {
      SetDrawBackgroundAsActive(false);
    }
  }
  ActionableView::OnGestureEvent(event);
}

void TrayBackgroundView::UpdateBackground(int alpha) {
  // The animator should never fire when the alternate shelf layout is used.
  if (!background_ || draw_background_as_active_)
    return;
  background_->set_alpha(hide_background_animator_.alpha() +
                         hover_background_animator_.alpha());
  SchedulePaint();
}

void TrayBackgroundView::SetContents(views::View* contents) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(contents);
}

void TrayBackgroundView::SetPaintsBackground(
    bool value, BackgroundAnimatorChangeType change_type) {
  hide_background_animator_.SetPaintsBackground(value, change_type);
}

void TrayBackgroundView::SetContentsBackground() {
  background_ = new TrayBackground(this);
  tray_container_->set_background(background_);
}

ShelfLayoutManager* TrayBackgroundView::GetShelfLayoutManager() {
  return status_area_widget()->shelf_widget()->shelf_layout_manager();
}

void TrayBackgroundView::SetShelfAlignment(ShelfAlignment alignment) {
  shelf_alignment_ = alignment;
  SetTrayBorder();
  tray_container_->SetAlignment(alignment);
}

void TrayBackgroundView::SetTrayBorder() {
  views::View* parent = status_area_widget_->status_area_widget_delegate();
  // Tray views are laid out right-to-left or bottom-to-top
  bool on_edge = (this == parent->child_at(0));
  int left_edge, top_edge, right_edge, bottom_edge;
  if (shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
    top_edge = ShelfLayoutManager::kShelfItemInset;
    left_edge = 0;
    bottom_edge = kShelfSize -
        ShelfLayoutManager::kShelfItemInset - kShelfItemHeight;
    right_edge = on_edge ? kPaddingFromEdgeOfShelf : 0;
  } else if (shelf_alignment() == SHELF_ALIGNMENT_LEFT) {
    top_edge = 0;
    left_edge = kShelfSize -
        ShelfLayoutManager::kShelfItemInset - kShelfItemHeight;
    bottom_edge = on_edge ? kPaddingFromEdgeOfShelf : 0;
    right_edge = ShelfLayoutManager::kShelfItemInset;
  } else { // SHELF_ALIGNMENT_RIGHT
    top_edge = 0;
    left_edge = ShelfLayoutManager::kShelfItemInset;
    bottom_edge = on_edge ? kPaddingFromEdgeOfShelf : 0;
    right_edge = kShelfSize -
        ShelfLayoutManager::kShelfItemInset - kShelfItemHeight;
  }
  SetBorder(views::Border::CreateEmptyBorder(
      top_edge, left_edge, bottom_edge, right_edge));
}

void TrayBackgroundView::OnImplicitAnimationsCompleted() {
  // If there is another animation in the queue, the reverse animation was
  // triggered before the completion of animating to invisible. Do not turn off
  // the visibility so that the next animation may render. The value of
  // layer()->GetTargetVisibility() can be incorrect if the hide animation was
  // aborted to schedule an animation to become visible. As the new animation
  // is not yet added to the queue. crbug.com/374236
  if(layer()->GetAnimator()->is_animating() ||
     layer()->GetTargetVisibility())
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
  if (shelf_alignment_ == SHELF_ALIGNMENT_BOTTOM ||
      shelf_alignment_ == SHELF_ALIGNMENT_TOP)
    transform.Translate(width(), 0.0f);
  else
    transform.Translate(0.0f, height());
  layer()->SetTransform(transform);
}

void TrayBackgroundView::InitializeBubbleAnimations(
    views::Widget* bubble_widget) {
  wm::SetWindowVisibilityAnimationType(
      bubble_widget->GetNativeWindow(),
      wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  wm::SetWindowVisibilityAnimationTransition(
      bubble_widget->GetNativeWindow(),
      wm::ANIMATE_HIDE);
  wm::SetWindowVisibilityAnimationDuration(
      bubble_widget->GetNativeWindow(),
      base::TimeDelta::FromMilliseconds(kAnimationDurationForPopupMs));
}

aura::Window* TrayBackgroundView::GetBubbleWindowContainer() const {
  return ash::Shell::GetContainer(
      tray_container()->GetWidget()->GetNativeWindow()->GetRootWindow(),
      ash::kShellWindowId_SettingBubbleContainer);
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
        rect.Inset(
            rtl ? kBubblePaddingHorizontalSide : 0,
            kBubblePaddingHorizontalBottom,
            rtl ? 0 : kBubblePaddingHorizontalSide,
            0);
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
    aura::Window* target_root = anchor_widget ?
        anchor_widget->GetNativeView()->GetRootWindow() :
        Shell::GetPrimaryRootWindow();
    rect = target_root->bounds();
    if (anchor_type == TrayBubbleView::ANCHOR_TYPE_TRAY) {
      if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
        rect = gfx::Rect(
            base::i18n::IsRTL() ?
            kPaddingFromRightEdgeOfScreenBottomAlignment :
            rect.width() - kPaddingFromRightEdgeOfScreenBottomAlignment,
            rect.height() - kPaddingFromBottomOfScreenBottomAlignment,
            0, 0);
        rect = ScreenUtil::ConvertRectToScreen(target_root, rect);
      } else if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_LEFT) {
        rect = gfx::Rect(
            kPaddingFromRightEdgeOfScreenBottomAlignment,
            rect.height() - kPaddingFromBottomOfScreenBottomAlignment,
            1, 1);
        rect = ScreenUtil::ConvertRectToScreen(target_root, rect);
      } else if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT) {
        rect = gfx::Rect(
            rect.width() - kPaddingFromRightEdgeOfScreenBottomAlignment,
            rect.height() - kPaddingFromBottomOfScreenBottomAlignment,
            1, 1);
        rect = ScreenUtil::ConvertRectToScreen(target_root, rect);
      } else {
        // TODO(bruthig) May need to handle other ANCHOR_ALIGNMENT_ values.
        // ie. ANCHOR_ALIGNMENT_TOP
        DCHECK(false) << "Unhandled anchor alignment.";
      }
    } else {
      rect = gfx::Rect(
          base::i18n::IsRTL() ?
          kPaddingFromRightEdgeOfScreenBottomAlignment :
          rect.width() - kPaddingFromRightEdgeOfScreenBottomAlignment,
          rect.height() - kPaddingFromBottomOfScreenBottomAlignment,
          0, 0);
    }
  }
  return rect;
}

TrayBubbleView::AnchorAlignment TrayBackgroundView::GetAnchorAlignment() const {
  switch (shelf_alignment_) {
    case SHELF_ALIGNMENT_BOTTOM:
      return TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM;
    case SHELF_ALIGNMENT_LEFT:
      return TrayBubbleView::ANCHOR_ALIGNMENT_LEFT;
    case SHELF_ALIGNMENT_RIGHT:
      return TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT;
    case SHELF_ALIGNMENT_TOP:
      return TrayBubbleView::ANCHOR_ALIGNMENT_TOP;
  }
  NOTREACHED();
  return TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM;
}

void TrayBackgroundView::SetDrawBackgroundAsActive(bool visible) {
  if (draw_background_as_active_ == visible)
    return;
  draw_background_as_active_ = visible;
  if (!background_)
    return;

  // Do not change gradually, changing color between grey and blue is weird.
  if (draw_background_as_active_)
    background_->set_color(kTrayBackgroundPressedColor);
  else if (hovered_)
    background_->set_alpha(kTrayBackgroundHoverAlpha);
  else
    background_->set_alpha(kTrayBackgroundAlpha);
  SchedulePaint();
}

void TrayBackgroundView::UpdateBubbleViewArrow(
    views::TrayBubbleView* bubble_view) {
  // Nothing to do here.
}

}  // namespace ash
