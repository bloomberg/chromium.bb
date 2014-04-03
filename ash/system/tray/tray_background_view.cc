// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include "ash/ash_switches.h"
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
#include "grit/ash_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace {

const int kTrayBackgroundAlpha = 100;
const int kTrayBackgroundHoverAlpha = 150;
const SkColor kTrayBackgroundPressedColor = SkColorSetRGB(66, 129, 244);

// Adjust the size of TrayContainer with additional padding.
const int kTrayContainerVerticalPaddingBottomAlignment  = 1;
const int kTrayContainerHorizontalPaddingBottomAlignment  = 1;
const int kTrayContainerVerticalPaddingVerticalAlignment  = 1;
const int kTrayContainerHorizontalPaddingVerticalAlignment = 1;

const int kAnimationDurationForPopupMS = 200;

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
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    leading_images_[kImageHorizontal][kImageTypeDefault] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_HORIZ_LEFT).ToImageSkia();
    middle_images_[kImageHorizontal][kImageTypeDefault] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_HORIZ_CENTER).ToImageSkia();
    trailing_images_[kImageHorizontal][kImageTypeDefault] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_HORIZ_RIGHT).ToImageSkia();

    leading_images_[kImageHorizontal][kImageTypeOnBlack] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_HORIZ_LEFT_ONBLACK).ToImageSkia();
    middle_images_[kImageHorizontal][kImageTypeOnBlack] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_HORIZ_CENTER_ONBLACK).ToImageSkia();
    trailing_images_[kImageHorizontal][kImageTypeOnBlack] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_HORIZ_RIGHT_ONBLACK).ToImageSkia();

    leading_images_[kImageHorizontal][kImageTypePressed] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_HORIZ_LEFT_PRESSED).ToImageSkia();
    middle_images_[kImageHorizontal][kImageTypePressed] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_HORIZ_CENTER_PRESSED).ToImageSkia();
    trailing_images_[kImageHorizontal][kImageTypePressed] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_HORIZ_RIGHT_PRESSED).ToImageSkia();

    leading_images_[kImageVertical][kImageTypeDefault] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_VERTICAL_TOP).ToImageSkia();
    middle_images_[kImageVertical][kImageTypeDefault] =
        rb.GetImageNamed(
            IDR_AURA_TRAY_BG_VERTICAL_CENTER).ToImageSkia();
    trailing_images_[kImageVertical][kImageTypeDefault] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_VERTICAL_BOTTOM).ToImageSkia();

    leading_images_[kImageVertical][kImageTypeOnBlack] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_VERTICAL_TOP_ONBLACK).ToImageSkia();
    middle_images_[kImageVertical][kImageTypeOnBlack] =
        rb.GetImageNamed(
            IDR_AURA_TRAY_BG_VERTICAL_CENTER_ONBLACK).ToImageSkia();
    trailing_images_[kImageVertical][kImageTypeOnBlack] =
        rb.GetImageNamed(
            IDR_AURA_TRAY_BG_VERTICAL_BOTTOM_ONBLACK).ToImageSkia();

    leading_images_[kImageVertical][kImageTypePressed] =
        rb.GetImageNamed(IDR_AURA_TRAY_BG_VERTICAL_TOP_PRESSED).ToImageSkia();
    middle_images_[kImageVertical][kImageTypePressed] =
        rb.GetImageNamed(
            IDR_AURA_TRAY_BG_VERTICAL_CENTER_PRESSED).ToImageSkia();
    trailing_images_[kImageVertical][kImageTypePressed] =
        rb.GetImageNamed(
            IDR_AURA_TRAY_BG_VERTICAL_BOTTOM_PRESSED).ToImageSkia();
  }

  virtual ~TrayBackground() {}

  SkColor color() { return color_; }
  void set_color(SkColor color) { color_ = color; }
  void set_alpha(int alpha) { color_ = SkColorSetARGB(alpha, 0, 0, 0); }

 private:
  ShelfWidget* GetShelfWidget() const {
    return RootWindowController::ForWindow(tray_background_view_->
        status_area_widget()->GetNativeWindow())->shelf();
  }

  void PaintForAlternateShelf(gfx::Canvas* canvas, views::View* view) const {
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

    const gfx::ImageSkia* leading = leading_images_[orientation][state];
    const gfx::ImageSkia* middle = middle_images_[orientation][state];
    const gfx::ImageSkia* trailing = trailing_images_[orientation][state];

    gfx::Rect bounds(view->GetLocalBounds());
    gfx::Point leading_location, trailing_location;
    gfx::Rect middle_bounds;

    if (orientation == kImageHorizontal) {
      leading_location = gfx::Point(0, 0);
      trailing_location = gfx::Point(bounds.width() - trailing->width(), 0);
      middle_bounds = gfx::Rect(
          leading->width(),
          0,
          bounds.width() - (leading->width() + trailing->width()),
          bounds.height());
    } else {
      leading_location = gfx::Point(0, 0);
      trailing_location = gfx::Point(0, bounds.height() - trailing->height());
      middle_bounds = gfx::Rect(
          0,
          leading->height(),
          bounds.width(),
          bounds.height() - (leading->height() + trailing->height()));
    }

    canvas->DrawImageInt(*leading,
                         leading_location.x(),
                         leading_location.y());

    canvas->DrawImageInt(*trailing,
                         trailing_location.x(),
                         trailing_location.y());

    canvas->TileImageInt(*middle,
                         middle_bounds.x(),
                         middle_bounds.y(),
                         middle_bounds.width(),
                         middle_bounds.height());
  }

  // Overridden from views::Background.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    if (ash::switches::UseAlternateShelfLayout()) {
      PaintForAlternateShelf(canvas, view);
    } else {
      SkPaint paint;
      paint.setAntiAlias(true);
      paint.setStyle(SkPaint::kFill_Style);
      paint.setColor(color_);
      SkPath path;
      gfx::Rect bounds(view->GetLocalBounds());
      SkScalar radius = SkIntToScalar(kTrayRoundedBorderRadius);
      path.addRoundRect(gfx::RectToSkRect(bounds), radius, radius);
      canvas->DrawPath(path, paint);
    }
  }

  SkColor color_;
  // Reference to the TrayBackgroundView for which this is a background.
  TrayBackgroundView* tray_background_view_;

  // References to the images used as backgrounds, they are owned by the
  // resource bundle class.
  const gfx::ImageSkia* leading_images_[kNumOrientations][kNumStates];
  const gfx::ImageSkia* middle_images_[kNumOrientations][kNumStates];
  const gfx::ImageSkia* trailing_images_[kNumOrientations][kNumStates];

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
    int vertical_padding = kTrayContainerVerticalPaddingBottomAlignment;
    int horizontal_padding = kTrayContainerHorizontalPaddingBottomAlignment;
    if (ash::switches::UseAlternateShelfLayout()) {
      vertical_padding = kPaddingFromEdgeOfShelf;
      horizontal_padding = kPaddingFromEdgeOfShelf;
    }
    SetBorder(views::Border::CreateEmptyBorder(vertical_padding,
                                               horizontal_padding,
                                               vertical_padding,
                                               horizontal_padding));

    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    layout->set_spread_blank_space(true);
    views::View::SetLayoutManager(layout);
  } else {
    int vertical_padding = kTrayContainerVerticalPaddingVerticalAlignment;
    int horizontal_padding = kTrayContainerHorizontalPaddingVerticalAlignment;
    if (ash::switches::UseAlternateShelfLayout()) {
      vertical_padding = kPaddingFromEdgeOfShelf;
      horizontal_padding = kPaddingFromEdgeOfShelf;
    }
    SetBorder(views::Border::CreateEmptyBorder(vertical_padding,
                                               horizontal_padding,
                                               vertical_padding,
                                               horizontal_padding));

    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
    layout->set_spread_blank_space(true);
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
}

TrayBackgroundView::~TrayBackgroundView() {
  if (GetWidget())
    GetWidget()->RemoveObserver(widget_observer_.get());
}

void TrayBackgroundView::Initialize() {
  GetWidget()->AddObserver(widget_observer_.get());
  SetTrayBorder();
}

const char* TrayBackgroundView::GetClassName() const {
  return kViewClassName;
}

void TrayBackgroundView::OnMouseEntered(const ui::MouseEvent& event) {
  hovered_ = true;
  if (!background_ || draw_background_as_active_ ||
      ash::switches::UseAlternateShelfLayout())
    return;
  hover_background_animator_.SetPaintsBackground(
      true, BACKGROUND_CHANGE_ANIMATE);
}

void TrayBackgroundView::OnMouseExited(const ui::MouseEvent& event) {
  hovered_ = false;
  if (!background_ || draw_background_as_active_ ||
      ash::switches::UseAlternateShelfLayout())
    return;
  hover_background_animator_.SetPaintsBackground(
      false, BACKGROUND_CHANGE_ANIMATE);
}

void TrayBackgroundView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void TrayBackgroundView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
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

void TrayBackgroundView::UpdateBackground(int alpha) {
  // The animator should never fire when the alternate shelf layout is used.
  if (!background_ || draw_background_as_active_)
    return;
  DCHECK(!ash::switches::UseAlternateShelfLayout());
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
  DCHECK(!ash::switches::UseAlternateShelfLayout());
  hide_background_animator_.SetPaintsBackground(value, change_type);
}

void TrayBackgroundView::SetContentsBackground() {
  background_ = new TrayBackground(this);
  tray_container_->set_background(background_);
}

ShelfLayoutManager* TrayBackgroundView::GetShelfLayoutManager() {
  return ShelfLayoutManager::ForShelf(GetWidget()->GetNativeView());
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
  if (ash::switches::UseAlternateShelfLayout()) {
    if (shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
      top_edge = ShelfLayoutManager::kShelfItemInset;
      left_edge = 0;
      bottom_edge = ShelfLayoutManager::GetPreferredShelfSize() -
          ShelfLayoutManager::kShelfItemInset - GetShelfItemHeight();
      right_edge = on_edge ? kPaddingFromEdgeOfShelf : 0;
    } else if (shelf_alignment() == SHELF_ALIGNMENT_LEFT) {
      top_edge = 0;
      left_edge = ShelfLayoutManager::GetPreferredShelfSize() -
          ShelfLayoutManager::kShelfItemInset - GetShelfItemHeight();
      bottom_edge = on_edge ? kPaddingFromEdgeOfShelf : 0;
      right_edge = ShelfLayoutManager::kShelfItemInset;
    } else { // SHELF_ALIGNMENT_RIGHT
      top_edge = 0;
      left_edge = ShelfLayoutManager::kShelfItemInset;
      bottom_edge = on_edge ? kPaddingFromEdgeOfShelf : 0;
      right_edge = ShelfLayoutManager::GetPreferredShelfSize() -
          ShelfLayoutManager::kShelfItemInset - GetShelfItemHeight();
    }
  } else {
    // Change the border padding for different shelf alignment.
    if (shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
      top_edge = 0;
      left_edge = 0;
      bottom_edge = on_edge ? kPaddingFromBottomOfScreenBottomAlignment :
          kPaddingFromBottomOfScreenBottomAlignment - 1;
      right_edge = on_edge ? kPaddingFromRightEdgeOfScreenBottomAlignment : 0;
    } else if (shelf_alignment() == SHELF_ALIGNMENT_TOP) {
      top_edge = on_edge ? kPaddingFromBottomOfScreenBottomAlignment :
          kPaddingFromBottomOfScreenBottomAlignment - 1;
      left_edge = 0;
      bottom_edge = 0;
      right_edge = on_edge ? kPaddingFromRightEdgeOfScreenBottomAlignment : 0;
    } else if (shelf_alignment() == SHELF_ALIGNMENT_LEFT) {
      top_edge = 0;
      left_edge = kPaddingFromOuterEdgeOfLauncherVerticalAlignment;
      bottom_edge = on_edge ? kPaddingFromBottomOfScreenVerticalAlignment : 0;
      right_edge = kPaddingFromInnerEdgeOfLauncherVerticalAlignment;
    } else {
      top_edge = 0;
      left_edge = kPaddingFromInnerEdgeOfLauncherVerticalAlignment;
      bottom_edge = on_edge ? kPaddingFromBottomOfScreenVerticalAlignment : 0;
      right_edge = kPaddingFromOuterEdgeOfLauncherVerticalAlignment;
    }
  }
  SetBorder(views::Border::CreateEmptyBorder(
      top_edge, left_edge, bottom_edge, right_edge));
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
      base::TimeDelta::FromMilliseconds(kAnimationDurationForPopupMS));
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
        if (!ash::switches::UseAlternateShelfLayout()) {
          rect.Inset(
              rtl ? kPaddingFromRightEdgeOfScreenBottomAlignment : 0,
              kTrayBubbleAnchorTopInsetBottomAnchor,
              rtl ? 0 : kPaddingFromRightEdgeOfScreenBottomAlignment,
              kPaddingFromBottomOfScreenBottomAlignment);
        } else {
          rect.Inset(
              rtl ? kAlternateLayoutBubblePaddingHorizontalSide : 0,
              kAlternateLayoutBubblePaddingHorizontalBottom,
              rtl ? 0 : kAlternateLayoutBubblePaddingHorizontalSide,
              0);
        }
      } else if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_LEFT) {
        if (!ash::switches::UseAlternateShelfLayout()) {
          rect.Inset(0, 0, kPaddingFromInnerEdgeOfLauncherVerticalAlignment + 5,
                     kPaddingFromBottomOfScreenVerticalAlignment);
        } else {
          rect.Inset(0, 0, kAlternateLayoutBubblePaddingVerticalSide + 4,
                     kAlternateLayoutBubblePaddingVerticalBottom);
        }
      } else {
        if (!ash::switches::UseAlternateShelfLayout()) {
          rect.Inset(kPaddingFromInnerEdgeOfLauncherVerticalAlignment + 1,
                     0, 0, kPaddingFromBottomOfScreenVerticalAlignment);
        } else {
          rect.Inset(kAlternateLayoutBubblePaddingVerticalSide, 0, 0,
                     kAlternateLayoutBubblePaddingVerticalBottom);
        }
      }
    } else if (anchor_type == TrayBubbleView::ANCHOR_TYPE_BUBBLE) {
      // Invert the offsets to align with the bubble below.
      // Note that with the alternate shelf layout the tips are not shown and
      // the offsets for left and right alignment do not need to be applied.
      int vertical_alignment = ash::switches::UseAlternateShelfLayout() ?
          0 :
          kPaddingFromInnerEdgeOfLauncherVerticalAlignment;
      int horizontal_alignment = ash::switches::UseAlternateShelfLayout() ?
          kAlternateLayoutBubblePaddingVerticalBottom :
          kPaddingFromBottomOfScreenVerticalAlignment;
      if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_LEFT)
        rect.Inset(vertical_alignment, 0, 0, horizontal_alignment);
      else if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT)
        rect.Inset(0, 0, vertical_alignment, horizontal_alignment);
    }
  }

  // TODO(jennyz): May need to add left/right alignment in the following code.
  if (rect.IsEmpty()) {
    aura::Window* target_root = anchor_widget ?
        anchor_widget->GetNativeView()->GetRootWindow() :
        Shell::GetPrimaryRootWindow();
    rect = target_root->bounds();
    rect = gfx::Rect(
        base::i18n::IsRTL() ? kPaddingFromRightEdgeOfScreenBottomAlignment :
        rect.width() - kPaddingFromRightEdgeOfScreenBottomAlignment,
        rect.height() - kPaddingFromBottomOfScreenBottomAlignment,
        0, 0);
    rect = ScreenUtil::ConvertRectToScreen(target_root, rect);
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
  draw_background_as_active_ = visible;
  if (!background_ || !switches::UseAlternateShelfLayout())
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
  if (switches::UseAlternateShelfLayout())
    return;

  aura::Window* root_window =
      bubble_view->GetWidget()->GetNativeView()->GetRootWindow();
  ash::ShelfLayoutManager* shelf = ShelfLayoutManager::ForShelf(root_window);
  bubble_view->SetArrowPaintType(
      (shelf && shelf->IsVisible()) ?
      views::BubbleBorder::PAINT_NORMAL :
      views::BubbleBorder::PAINT_TRANSPARENT);
}

}  // namespace ash
