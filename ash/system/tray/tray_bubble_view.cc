// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_bubble_view.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/aura/window.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/events/event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace internal {

// Inset the arrow a bit from the edge.
const int kArrowMinOffset = 20;
const int kBubbleSpacing = 20;

const int kAnimationDurationForPopupMS = 200;

// Custom border for TrayBubbleView. Contains special logic for GetBounds()
// to stack bubbles with no arrows correctly. Also calculates the arrow offset.
class TrayBubbleBorder : public views::BubbleBorder {
 public:
  TrayBubbleBorder(views::View* owner,
                   views::View* anchor,
                   views::BubbleBorder::ArrowLocation arrow_location,
                   TrayBubbleView::InitParams params)
      : views::BubbleBorder(arrow_location, params.shadow),
        owner_(owner),
        anchor_(anchor),
        tray_arrow_offset_(params.arrow_offset) {
    set_alignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
    set_background_color(params.arrow_color);
  }

  virtual ~TrayBubbleBorder() {}

  // Overridden from views::BubbleBorder.
  // Override views::BubbleBorder to set the bubble on top of the anchor when
  // it has no arrow.
  virtual gfx::Rect GetBounds(const gfx::Rect& position_relative_to,
                              const gfx::Size& contents_size) const OVERRIDE {
    if (arrow_location() != NONE) {
      return views::BubbleBorder::GetBounds(position_relative_to,
                                            contents_size);
    }

    gfx::Size border_size(contents_size);
    gfx::Insets insets;
    GetInsets(&insets);
    border_size.Enlarge(insets.width(), insets.height());

    const int x = position_relative_to.x() +
        position_relative_to.width() / 2 - border_size.width() / 2;
    // Position the bubble on top of the anchor.
    const int y = position_relative_to.y() - border_size.height()
        + insets.height() - kBubbleSpacing;
    return gfx::Rect(x, y, border_size.width(), border_size.height());
  }

  void UpdateArrowOffset() {
    int arrow_offset = 0;
    if (arrow_location() == views::BubbleBorder::BOTTOM_RIGHT ||
        arrow_location() == views::BubbleBorder::BOTTOM_LEFT) {
      // Note: tray_arrow_offset_ is relative to the anchor widget.
      if (tray_arrow_offset_ ==
          internal::TrayBubbleView::InitParams::kArrowDefaultOffset) {
        arrow_offset = kArrowMinOffset;
      } else {
        const int width = owner_->GetWidget()->GetContentsView()->width();
        gfx::Point pt(tray_arrow_offset_, 0);
        views::View::ConvertPointToScreen(
            anchor_->GetWidget()->GetRootView(), &pt);
        views::View::ConvertPointFromScreen(
            owner_->GetWidget()->GetRootView(), &pt);
        arrow_offset = pt.x();
        if (arrow_location() == views::BubbleBorder::BOTTOM_RIGHT)
          arrow_offset = width - arrow_offset;
        arrow_offset = std::max(arrow_offset, kArrowMinOffset);
      }
    } else {
      if (tray_arrow_offset_ ==
          internal::TrayBubbleView::InitParams::kArrowDefaultOffset) {
        arrow_offset = kArrowMinOffset;
      } else {
        gfx::Point pt(0, tray_arrow_offset_);
        views::View::ConvertPointToScreen(
            anchor_->GetWidget()->GetRootView(), &pt);
        views::View::ConvertPointFromScreen(
            owner_->GetWidget()->GetRootView(), &pt);
        arrow_offset = pt.y();
        arrow_offset = std::max(arrow_offset, kArrowMinOffset);
      }
    }
    set_arrow_offset(arrow_offset);
  }

 private:
  views::View* owner_;
  views::View* anchor_;
  const int tray_arrow_offset_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleBorder);
};

// Custom background for TrayBubbleView. Fills in the top and bottom margins
// with appropriate background colors without overwriting the rounded corners.
class TrayBubbleBackground : public views::Background {
 public:
  explicit TrayBubbleBackground(views::BubbleBorder* border,
                                SkColor top_color,
                                SkColor bottom_color)
      : border_(border),
        top_color_(top_color),
        bottom_color_(bottom_color),
        radius_(SkIntToScalar(border->GetBorderCornerRadius() - 1)) {
  }

  SkScalar radius() const { return radius_; }

  // Overridden from Background:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    canvas->Save();

    // Set a clip mask for the bubble's rounded corners.
    gfx::Rect bounds(view->GetContentsBounds());
    const int border_thickness(border_->GetBorderThickness());
    bounds.Inset(-border_thickness, -border_thickness);
    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(bounds), radius_, radius_);
    canvas->ClipPath(path);

    // Paint the header and footer (assumes the bubble contents fill in their
    // own backgrounds).
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);

    gfx::Rect top_rect(bounds);
    top_rect.set_height(radius_);
    paint.setColor(top_color_);
    canvas->DrawRect(top_rect, paint);

    gfx::Rect bottom_rect(bounds);
    bottom_rect.set_y(bounds.y() + (bounds.height() - radius_));
    bottom_rect.set_height(radius_);
    paint.setColor(bottom_color_);
    canvas->DrawRect(bottom_rect, paint);

    canvas->Restore();
  }

 private:
  views::BubbleBorder* border_;
  SkColor top_color_;
  SkColor bottom_color_;
  SkScalar radius_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleBackground);
};

// Custom layout for the bubble-view. Does the default box-layout if there is
// enough height. Otherwise, makes sure the bottom rows are visible.
class BottomAlignedBoxLayout : public views::BoxLayout {
 public:
  explicit BottomAlignedBoxLayout(internal::TrayBubbleView* bubble_view)
      : views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0),
        bubble_view_(bubble_view) {
  }

  virtual ~BottomAlignedBoxLayout() {}

 private:
  virtual void Layout(views::View* host) OVERRIDE {
    if (host->height() >= host->GetPreferredSize().height() ||
        !bubble_view_->is_gesture_dragging()) {
      views::BoxLayout::Layout(host);
      return;
    }

    int consumed_height = 0;
    for (int i = host->child_count() - 1;
        i >= 0 && consumed_height < host->height(); --i) {
      views::View* child = host->child_at(i);
      if (!child->visible())
        continue;
      gfx::Size size = child->GetPreferredSize();
      child->SetBounds(0, host->height() - consumed_height - size.height(),
          host->width(), size.height());
      consumed_height += size.height();
    }
  }

  internal::TrayBubbleView* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(BottomAlignedBoxLayout);
};

// static
const int TrayBubbleView::InitParams::kArrowDefaultOffset = -1;

TrayBubbleView::InitParams::InitParams(AnchorType anchor_type,
                                       ShelfAlignment shelf_alignment)
    : anchor_type(anchor_type),
      shelf_alignment(shelf_alignment),
      bubble_width(kTrayPopupWidth),
      max_height(0),
      can_activate(false),
      close_on_deactivate(true),
      top_color(SK_ColorBLACK),
      arrow_color(SK_ColorBLACK),
      arrow_offset(kArrowDefaultOffset),
      shadow(views::BubbleBorder::BIG_SHADOW) {
}

TrayBubbleView* TrayBubbleView::Create(views::View* anchor,
                                       Host* host,
                                       const InitParams& init_params) {
  // Set arrow_location here so that it can be passed correctly to the
  // BubbleView constructor.
  views::BubbleBorder::ArrowLocation arrow_location;
  if (init_params.anchor_type == ANCHOR_TYPE_TRAY) {
    if (init_params.shelf_alignment == SHELF_ALIGNMENT_BOTTOM) {
      arrow_location = base::i18n::IsRTL() ?
          views::BubbleBorder::BOTTOM_LEFT : views::BubbleBorder::BOTTOM_RIGHT;
    } else if (init_params.shelf_alignment == SHELF_ALIGNMENT_LEFT) {
      arrow_location = views::BubbleBorder::LEFT_BOTTOM;
    } else {
      arrow_location = views::BubbleBorder::RIGHT_BOTTOM;
    }
  } else {
    arrow_location = views::BubbleBorder::NONE;
  }

  return new TrayBubbleView(init_params, arrow_location, anchor, host);
}

TrayBubbleView::TrayBubbleView(
    const InitParams& init_params,
    views::BubbleBorder::ArrowLocation arrow_location,
    views::View* anchor,
    Host* host)
    : views::BubbleDelegateView(anchor, arrow_location),
      params_(init_params),
      host_(host),
      bubble_border_(NULL),
      bubble_background_(NULL),
      is_gesture_dragging_(false) {
  set_parent_window(Shell::GetContainer(
      anchor->GetWidget()->GetNativeWindow()->GetRootWindow(),
      internal::kShellWindowId_SettingBubbleContainer));
  set_notify_enter_exit_on_child(true);
  set_close_on_deactivate(init_params.close_on_deactivate);
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(true);

  bubble_border_ = new TrayBubbleBorder(
      this, anchor_view(), arrow_location, params_);

  bubble_background_ = new TrayBubbleBackground(
    bubble_border_, init_params.top_color, init_params.arrow_color);

  // Inset the view on the top and bottom by the corner radius to avoid drawing
  // over the the bubble corners.
  const int radius = bubble_background_->radius();
  set_margins(gfx::Insets(radius, 0, radius, 0));
}

TrayBubbleView::~TrayBubbleView() {
  // Inform host items (models) that their views are being destroyed.
  if (host_)
    host_->BubbleViewDestroyed();
}

void TrayBubbleView::UpdateBubble() {
  SizeToContents();
  GetWidget()->GetRootView()->SchedulePaint();
}

void TrayBubbleView::SetMaxHeight(int height) {
  params_.max_height = height;
  if (GetWidget())
    SizeToContents();
}

void TrayBubbleView::Init() {
  views::BoxLayout* layout = new BottomAlignedBoxLayout(this);
  layout->set_spread_blank_space(true);
  SetLayoutManager(layout);
}

gfx::Rect TrayBubbleView::GetAnchorRect() {
  gfx::Rect rect;

  if (anchor_widget() && anchor_widget()->IsVisible()) {
    rect = anchor_widget()->GetWindowBoundsInScreen();
    if (params_.anchor_type == ANCHOR_TYPE_TRAY) {
      if (params_.shelf_alignment == SHELF_ALIGNMENT_BOTTOM) {
        bool rtl = base::i18n::IsRTL();
        rect.Inset(
            rtl ? kPaddingFromRightEdgeOfScreenBottomAlignment : 0,
            0,
            rtl ? 0 : kPaddingFromRightEdgeOfScreenBottomAlignment,
            kPaddingFromBottomOfScreenBottomAlignment);
      } else if (params_.shelf_alignment == SHELF_ALIGNMENT_LEFT) {
        rect.Inset(0, 0, kPaddingFromInnerEdgeOfLauncherVerticalAlignment,
                   kPaddingFromBottomOfScreenVerticalAlignment);
      } else {
        rect.Inset(kPaddingFromInnerEdgeOfLauncherVerticalAlignment,
                   0, 0, kPaddingFromBottomOfScreenVerticalAlignment);
      }
    } else if (params_.anchor_type == ANCHOR_TYPE_BUBBLE) {
      // Invert the offsets to align with the bubble below.
      if (params_.shelf_alignment == SHELF_ALIGNMENT_LEFT) {
        rect.Inset(kPaddingFromInnerEdgeOfLauncherVerticalAlignment,
                   0, 0, kPaddingFromBottomOfScreenVerticalAlignment);
      } else if (params_.shelf_alignment == SHELF_ALIGNMENT_RIGHT) {
        rect.Inset(0, 0, kPaddingFromInnerEdgeOfLauncherVerticalAlignment,
                   kPaddingFromBottomOfScreenVerticalAlignment);
      }
    }
  }

  // TODO(jennyz): May need to add left/right alignment in the following code.
  if (rect.IsEmpty()) {
    rect = gfx::Screen::GetPrimaryDisplay().bounds();
    rect = gfx::Rect(
        base::i18n::IsRTL() ? kPaddingFromRightEdgeOfScreenBottomAlignment :
        rect.width() - kPaddingFromRightEdgeOfScreenBottomAlignment,
        rect.height() - kPaddingFromBottomOfScreenBottomAlignment,
        0, 0);
  }
  return rect;
}

bool TrayBubbleView::CanActivate() const {
  return params_.can_activate;
}

// Overridden to create BubbleFrameView and set a custom border and background.
views::NonClientFrameView* TrayBubbleView::CreateNonClientFrameView(
    views::Widget* widget) {
  views::BubbleFrameView* bubble_frame_view =
      new views::BubbleFrameView(margins(), bubble_border_);
  bubble_frame_view->set_background(bubble_background_);
  return bubble_frame_view;
}

bool TrayBubbleView::WidgetHasHitTestMask() const {
  return true;
}

void TrayBubbleView::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  mask->addRect(gfx::RectToSkRect(GetBubbleFrameView()->GetContentsBounds()));
}

gfx::Size TrayBubbleView::GetPreferredSize() {
  gfx::Size size = views::BubbleDelegateView::GetPreferredSize();
  int height = size.height();
  if (params_.max_height != 0 && height > params_.max_height)
    height = params_.max_height;
  return gfx::Size(params_.bubble_width, height);
}

void TrayBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  if (host_)
    host_->OnMouseEnteredView();
}

void TrayBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  if (host_)
    host_->OnMouseExitedView();
}

void TrayBubbleView::GetAccessibleState(ui::AccessibleViewState* state) {
  if (params_.can_activate) {
    state->role = ui::AccessibilityTypes::ROLE_WINDOW;
    state->name = host_->GetAccessibleName();
  }
}

void TrayBubbleView::ChildPreferredSizeChanged(View* child) {
  SizeToContents();
}

void TrayBubbleView::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (is_add && child == this) {
    parent->SetPaintToLayer(true);
    parent->SetFillsBoundsOpaquely(true);
    parent->layer()->SetMasksToBounds(true);
  }
}

TrayBubbleView::Host::Host()
    : widget_(NULL),
      bubble_view_(NULL),
      tray_view_(NULL) {
  Shell::GetInstance()->AddEnvEventFilter(this);
}

TrayBubbleView::Host::~Host() {
  Shell::GetInstance()->RemoveEnvEventFilter(this);
}

void TrayBubbleView::Host::InitializeAndShowBubble(views::Widget* widget,
                                                   TrayBubbleView* bubble_view,
                                                   views::View* tray_view) {
  widget_ = widget;
  bubble_view_ = bubble_view;
  tray_view_ = tray_view;

  // Must occur after call to BubbleDelegateView::CreateBubble().
  bubble_view->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);

  bubble_view->bubble_border()->UpdateArrowOffset();

  // Setup animation.
  ash::SetWindowVisibilityAnimationType(
      widget->GetNativeWindow(),
      ash::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ash::SetWindowVisibilityAnimationTransition(
      widget->GetNativeWindow(),
      ash::ANIMATE_BOTH);
  ash::SetWindowVisibilityAnimationDuration(
      widget->GetNativeWindow(),
      base::TimeDelta::FromMilliseconds(kAnimationDurationForPopupMS));

  bubble_view->Show();
  bubble_view->UpdateBubble();
}

bool TrayBubbleView::Host::PreHandleKeyEvent(aura::Window* target,
                                             ui::KeyEvent* event) {
  return false;
}

bool TrayBubbleView::Host::PreHandleMouseEvent(aura::Window* target,
                                               ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessLocatedEvent(target, *event);
  return false;
}

ui::TouchStatus TrayBubbleView::Host::PreHandleTouchEvent(
    aura::Window* target,
    ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    ProcessLocatedEvent(target, *event);
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult TrayBubbleView::Host::PreHandleGestureEvent(
    aura::Window* target,
    ui::GestureEvent* event) {
  return ui::ER_UNHANDLED;
}

void TrayBubbleView::Host::ProcessLocatedEvent(
    aura::Window* target, const ui::LocatedEvent& event) {
  if (target) {
    // Don't process events that occurred inside an embedded menu.
    RootWindowController* root_controller =
        GetRootWindowController(target->GetRootWindow());
    if (root_controller && root_controller->GetContainer(
            ash::internal::kShellWindowId_MenuContainer)->Contains(target)) {
      return;
    }
  }
  if (!widget_)
    return;
  gfx::Rect bounds = widget_->GetWindowBoundsInScreen();
  gfx::Insets insets;
  bubble_view_->bubble_border()->GetInsets(&insets);
  bounds.Inset(insets);
  if (bounds.Contains(event.root_location()))
    return;
  if (tray_view_) {
    // If the user clicks on the parent tray, don't process the event here,
    // let the tray logic handle the event and determine show/hide behavior.
    bounds = tray_view_->GetWidget()->GetClientAreaBoundsInScreen();
    if (bounds.Contains(event.root_location()))
      return;
  }
  // Handle clicking outside the bubble and tray. We don't block the event, so
  // it will also be handled by whatever widget was clicked on.
  OnClickedOutsideView();
}


}  // namespace internal
}  // namespace ash
