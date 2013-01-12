// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include "ash/launcher/background_animator.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace {

const SkColor kTrayBackgroundAlpha = 100;
const SkColor kTrayBackgroundHoverAlpha = 150;

// Adjust the size of TrayContainer with additional padding.
const int kTrayContainerVerticalPaddingBottomAlignment  = 1;
const int kTrayContainerHorizontalPaddingBottomAlignment  = 1;
const int kTrayContainerVerticalPaddingVerticalAlignment  = 1;
const int kTrayContainerHorizontalPaddingVerticalAlignment = 1;

const int kAnimationDurationForPopupMS = 200;

}  // namespace

using views::TrayBubbleView;

namespace ash {
namespace internal {

// Used to track when the anchor widget changes position on screen so that the
// bubble position can be updated.
class TrayBackgroundView::TrayWidgetObserver : public views::WidgetObserver {
 public:
  explicit TrayWidgetObserver(TrayBackgroundView* host)
      : host_(host) {
  }

  virtual void OnWidgetBoundsChanged(views::Widget* widget,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    host_->AnchorUpdated();
  }

  virtual void OnWidgetVisibilityChanged(views::Widget* widget,
                                         bool visible) OVERRIDE {
    host_->AnchorUpdated();
  }

 private:
  TrayBackgroundView* host_;

  DISALLOW_COPY_AND_ASSIGN(TrayWidgetObserver);
};

class TrayBackground : public views::Background {
 public:
  TrayBackground() : alpha_(kTrayBackgroundAlpha) {}
  virtual ~TrayBackground() {}

  void set_alpha(int alpha) { alpha_ = alpha; }

 private:
  // Overridden from views::Background.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SkColorSetARGB(alpha_, 0, 0, 0));
    SkPath path;
    gfx::Rect bounds(view->GetLocalBounds());
    SkScalar radius = SkIntToScalar(kTrayRoundedBorderRadius);
    path.addRoundRect(gfx::RectToSkRect(bounds), radius, radius);
    canvas->DrawPath(path, paint);
  }

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

gfx::Size TrayBackgroundView::TrayContainer::GetPreferredSize() {
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

void TrayBackgroundView::TrayContainer::ViewHierarchyChanged(bool is_add,
                                                             View* parent,
                                                             View* child) {
  if (parent == this)
    PreferredSizeChanged();
}

void TrayBackgroundView::TrayContainer::UpdateLayout() {
  // Adjust the size of status tray dark background by adding additional
  // empty border.
  if (alignment_ == SHELF_ALIGNMENT_BOTTOM ||
      alignment_ == SHELF_ALIGNMENT_TOP) {
    set_border(views::Border::CreateEmptyBorder(
        kTrayContainerVerticalPaddingBottomAlignment,
        kTrayContainerHorizontalPaddingBottomAlignment,
        kTrayContainerVerticalPaddingBottomAlignment,
        kTrayContainerHorizontalPaddingBottomAlignment));
    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    layout->set_spread_blank_space(true);
    views::View::SetLayoutManager(layout);
  } else {
    set_border(views::Border::CreateEmptyBorder(
        kTrayContainerVerticalPaddingVerticalAlignment,
        kTrayContainerHorizontalPaddingVerticalAlignment,
        kTrayContainerVerticalPaddingVerticalAlignment,
        kTrayContainerHorizontalPaddingVerticalAlignment));
    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
    layout->set_spread_blank_space(true);
    views::View::SetLayoutManager(layout);
  }
  PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// TrayBackgroundView

TrayBackgroundView::TrayBackgroundView(
    internal::StatusAreaWidget* status_area_widget)
    : status_area_widget_(status_area_widget),
      tray_container_(NULL),
      shelf_alignment_(SHELF_ALIGNMENT_BOTTOM),
      background_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(hide_background_animator_(
          this, 0, kTrayBackgroundAlpha)),
      ALLOW_THIS_IN_INITIALIZER_LIST(hover_background_animator_(
          this, 0, kTrayBackgroundHoverAlpha - kTrayBackgroundAlpha)),
      ALLOW_THIS_IN_INITIALIZER_LIST(widget_observer_(
          new TrayWidgetObserver(this))) {
  set_notify_enter_exit_on_child(true);

  // Initially we want to paint the background, but without the hover effect.
  SetPaintsBackground(true, internal::BackgroundAnimator::CHANGE_IMMEDIATE);
  hover_background_animator_.SetPaintsBackground(false,
      internal::BackgroundAnimator::CHANGE_IMMEDIATE);

  tray_container_ = new TrayContainer(shelf_alignment_);
  SetContents(tray_container_);
}

TrayBackgroundView::~TrayBackgroundView() {
  if (GetWidget())
    GetWidget()->RemoveObserver(widget_observer_.get());
}

void TrayBackgroundView::Initialize() {
  GetWidget()->AddObserver(widget_observer_.get());
  SetBorder();
}

void TrayBackgroundView::OnMouseEntered(const ui::MouseEvent& event) {
  hover_background_animator_.SetPaintsBackground(true,
      internal::BackgroundAnimator::CHANGE_ANIMATE);
}

void TrayBackgroundView::OnMouseExited(const ui::MouseEvent& event) {
  hover_background_animator_.SetPaintsBackground(false,
      internal::BackgroundAnimator::CHANGE_ANIMATE);
}

void TrayBackgroundView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void TrayBackgroundView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // The tray itself expands to the right and bottom edge of the screen to make
  // sure clicking on the edges brings up the popup. However, the focus border
  // should be only around the container.
  if (HasFocus() && (focusable() || IsAccessibilityFocusable()))
    DrawBorder(canvas, GetContentsBounds());
}

void TrayBackgroundView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
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

void TrayBackgroundView::UpdateBackground(int alpha) {
  if (background_) {
    background_->set_alpha(hide_background_animator_.alpha() +
                           hover_background_animator_.alpha());
  }
  SchedulePaint();
}

void TrayBackgroundView::SetContents(views::View* contents) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(contents);
}

void TrayBackgroundView::SetContentsBackground() {
  background_ = new internal::TrayBackground;
  tray_container_->set_background(background_);
}

void TrayBackgroundView::SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type) {
  hide_background_animator_.SetPaintsBackground(value, change_type);
}

ShelfLayoutManager* TrayBackgroundView::GetShelfLayoutManager() {
  return
      RootWindowController::ForLauncher(GetWidget()->GetNativeView())->shelf();
}

void TrayBackgroundView::SetShelfAlignment(ShelfAlignment alignment) {
  shelf_alignment_ = alignment;
  SetBorder();
  tray_container_->SetAlignment(alignment);
}

void TrayBackgroundView::SetBorder() {
  views::View* parent = status_area_widget_->status_area_widget_delegate();
  // Tray views are laid out right-to-left or bottom-to-top
  int on_edge = (this == parent->child_at(0));
  // Change the border padding for different shelf alignment.
  if (shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
    set_border(views::Border::CreateEmptyBorder(
        0, 0,
        on_edge ? kPaddingFromBottomOfScreenBottomAlignment :
                  kPaddingFromBottomOfScreenBottomAlignment - 1,
        on_edge ? kPaddingFromRightEdgeOfScreenBottomAlignment : 0));
  } else if (shelf_alignment() == SHELF_ALIGNMENT_TOP) {
    set_border(views::Border::CreateEmptyBorder(
        on_edge ? kPaddingFromBottomOfScreenBottomAlignment :
                  kPaddingFromBottomOfScreenBottomAlignment - 1,
        0, 0,
        on_edge ? kPaddingFromRightEdgeOfScreenBottomAlignment : 0));
  } else if (shelf_alignment() == SHELF_ALIGNMENT_LEFT) {
    set_border(views::Border::CreateEmptyBorder(
        0, kPaddingFromOuterEdgeOfLauncherVerticalAlignment,
        on_edge ? kPaddingFromBottomOfScreenVerticalAlignment : 0,
        kPaddingFromInnerEdgeOfLauncherVerticalAlignment));
  } else {
    set_border(views::Border::CreateEmptyBorder(
        0, kPaddingFromInnerEdgeOfLauncherVerticalAlignment,
        on_edge ? kPaddingFromBottomOfScreenVerticalAlignment : 0,
        kPaddingFromOuterEdgeOfLauncherVerticalAlignment));
  }
}

void TrayBackgroundView::InitializeBubbleAnimations(
    views::Widget* bubble_widget) {
  views::corewm::SetWindowVisibilityAnimationType(
      bubble_widget->GetNativeWindow(),
      views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  views::corewm::SetWindowVisibilityAnimationTransition(
      bubble_widget->GetNativeWindow(),
      views::corewm::ANIMATE_HIDE);
  views::corewm::SetWindowVisibilityAnimationDuration(
      bubble_widget->GetNativeWindow(),
      base::TimeDelta::FromMilliseconds(kAnimationDurationForPopupMS));
}

aura::Window* TrayBackgroundView::GetBubbleWindowContainer() const {
  return ash::Shell::GetContainer(
      tray_container()->GetWidget()->GetNativeWindow()->GetRootWindow(),
      ash::internal::kShellWindowId_SettingBubbleContainer);
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
            rtl ? kPaddingFromRightEdgeOfScreenBottomAlignment : 0,
            4,
            rtl ? 0 : kPaddingFromRightEdgeOfScreenBottomAlignment,
            kPaddingFromBottomOfScreenBottomAlignment);
      } else if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_LEFT) {
        rect.Inset(0, 0, kPaddingFromInnerEdgeOfLauncherVerticalAlignment + 5,
                   kPaddingFromBottomOfScreenVerticalAlignment);
      } else {
        rect.Inset(kPaddingFromInnerEdgeOfLauncherVerticalAlignment + 1,
                   0, 0, kPaddingFromBottomOfScreenVerticalAlignment);
      }
    } else if (anchor_type == TrayBubbleView::ANCHOR_TYPE_BUBBLE) {
      // Invert the offsets to align with the bubble below.
      if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_LEFT) {
        rect.Inset(kPaddingFromInnerEdgeOfLauncherVerticalAlignment,
                   0, 0, kPaddingFromBottomOfScreenVerticalAlignment);
      } else if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT) {
        rect.Inset(0, 0, kPaddingFromInnerEdgeOfLauncherVerticalAlignment,
                   kPaddingFromBottomOfScreenVerticalAlignment);
      }
    }
  }

  // TODO(jennyz): May need to add left/right alignment in the following code.
  if (rect.IsEmpty()) {
    rect = Shell::GetScreen()->GetPrimaryDisplay().bounds();
    rect = gfx::Rect(
        base::i18n::IsRTL() ? kPaddingFromRightEdgeOfScreenBottomAlignment :
        rect.width() - kPaddingFromRightEdgeOfScreenBottomAlignment,
        rect.height() - kPaddingFromBottomOfScreenBottomAlignment,
        0, 0);
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

void TrayBackgroundView::UpdateBubbleViewArrow(
    views::TrayBubbleView* bubble_view) {
  aura::RootWindow* root_window =
      bubble_view->GetWidget()->GetNativeView()->GetRootWindow();
  ash::internal::ShelfLayoutManager* shelf =
      ash::GetRootWindowController(root_window)->shelf();
  bubble_view->SetPaintArrow(shelf->IsVisible());
}

}  // namespace internal
}  // namespace ash
