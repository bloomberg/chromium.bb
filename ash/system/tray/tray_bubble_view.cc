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
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const int kShadowThickness = 4;
const int kBottomLineHeight = 1;
const int kSystemTrayBubbleHorizontalInset = 1;
const int kSystemTrayBubbleVerticalInset = 1;

const int kArrowHeight = 9;
const int kArrowWidth = 19;

// Inset the arrow a bit from the edge.
const int kArrowEdgeMargin = 12;
const int kArrowMinOffset = kArrowWidth / 2 + kArrowEdgeMargin;

const SkColor kShadowColor = SkColorSetARGB(0xff, 0, 0, 0);

const int kAnimationDurationForPopupMS = 200;

void DrawBlurredShadowAroundView(gfx::Canvas* canvas,
                                 int top,
                                 int bottom,
                                 int width,
                                 const gfx::Insets& inset) {
  SkPath path;
  path.incReserve(4);
  path.moveTo(SkIntToScalar(inset.left() + kShadowThickness),
              SkIntToScalar(top + kShadowThickness + 1));
  path.lineTo(SkIntToScalar(inset.left() + kShadowThickness),
              SkIntToScalar(bottom));
  path.lineTo(SkIntToScalar(width),
              SkIntToScalar(bottom));
  path.lineTo(SkIntToScalar(width),
              SkIntToScalar(top + kShadowThickness + 1));

  SkPaint paint;
  paint.setColor(kShadowColor);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  paint.setStrokeWidth(SkIntToScalar(3));
  paint.setImageFilter(new SkBlurImageFilter(
      SkIntToScalar(3), SkIntToScalar(3)))->unref();
  canvas->sk_canvas()->drawPath(path, paint);
}

class TrayBubbleBorder : public views::BubbleBorder {
 public:
  TrayBubbleBorder(views::View* owner,
                   views::View* anchor,
                   views::BubbleBorder::ArrowLocation arrow_location,
                   int arrow_offset,
                   const SkColor& arrow_color)
      : views::BubbleBorder(arrow_location, views::BubbleBorder::NO_SHADOW),
        owner_(owner),
        anchor_(anchor),
        tray_arrow_offset_(arrow_offset) {
    set_alignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
    set_background_color(arrow_color);
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

    const int kArrowOverlap = 3;
    int x = position_relative_to.x() +
        position_relative_to.width() / 2 - border_size.width() / 2;
    // Position the bubble on top of the anchor.
    int y = position_relative_to.y() +
        kArrowOverlap - border_size.height();
    return gfx::Rect(x, y, border_size.width(), border_size.height());
  }

  // TrayBubbleView supports dynamically updated bubbles. This does not
  // behave well with BubbleFrameView which expects arrow_location to be
  // unmirrored during initial layout (when ClientView is constructed),
  // then mirrored after SizeToContents() gets called.
  // So, instead of mirroring the arrow in CreateNonClientFrameView,
  // mirror it here instead.
  // TODO(stevenjb): Fix this in ui/views/bubble: crbug.com/139813
  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE {
    ArrowLocation arrow_loc = arrow_location();
    if (base::i18n::IsRTL())
      arrow_loc = horizontal_mirror(arrow_loc);
    return GetInsetsForArrowLocation(insets, arrow_loc);
  }

  // Overridden from views::Border.
  virtual void Paint(const views::View& view,
                     gfx::Canvas* canvas) const OVERRIDE {
    gfx::Insets inset;
    // Get the unmirrored insets for the arrow location; the tray bubbles are
    // never mirrored for RTL (since that would put them off screen).
    GetInsetsForArrowLocation(&inset, arrow_location());
    DrawBlurredShadowAroundView(
        canvas, 0, owner_->height(), owner_->width(), inset);

    // Draw the bottom line.
    int y = owner_->height() + inset.top();
    canvas->FillRect(gfx::Rect(inset.left(), y, owner_->width(),
                               kBottomLineHeight), kBorderDarkColor);

    if (!Shell::GetInstance()->shelf()->IsVisible() ||
        arrow_location() == views::BubbleBorder::NONE)
      return;

    gfx::Point arrow_reference;

    // Draw the arrow after drawing child borders, so that the arrow can cover
    // its overlap section with child border.
    SkPath path;
    path.incReserve(4);
    if (arrow_location() == views::BubbleBorder::BOTTOM_RIGHT ||
        arrow_location() == views::BubbleBorder::BOTTOM_LEFT) {
      // Note: tray_arrow_offset_ is relative to the anchor widget.
      int tip_x;
      if (tray_arrow_offset_ ==
          internal::TrayBubbleView::InitParams::kArrowDefaultOffset) {
        if (arrow_location() == views::BubbleBorder::BOTTOM_LEFT)
          tip_x = kArrowMinOffset;
        else
          tip_x = owner_->width() - kArrowMinOffset;
      } else {
        gfx::Point pt(tray_arrow_offset_, 0);
        views::View::ConvertPointToScreen(
            anchor_->GetWidget()->GetRootView(), &pt);
        views::View::ConvertPointFromScreen(
            owner_->GetWidget()->GetRootView(), &pt);
        tip_x = std::min(pt.x(), owner_->width() - kArrowMinOffset);
        tip_x = std::max(tip_x, kArrowMinOffset);
      }
      int left_base_x = tip_x - kArrowWidth / 2;
      int left_base_y = y;
      int tip_y = left_base_y + kArrowHeight;
      path.moveTo(SkIntToScalar(left_base_x), SkIntToScalar(left_base_y));
      path.lineTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
      path.lineTo(SkIntToScalar(left_base_x + kArrowWidth),
                  SkIntToScalar(left_base_y));
      arrow_reference.SetPoint(tip_x, left_base_y - kArrowHeight);
    } else {
      int tip_y;
      if (tray_arrow_offset_ ==
          internal::TrayBubbleView::InitParams::kArrowDefaultOffset) {
        tip_y = owner_->height() - kArrowMinOffset;
      } else {
        int pty = y - tray_arrow_offset_;
        gfx::Point pt(0, pty);
        views::View::ConvertPointToScreen(
            anchor_->GetWidget()->GetRootView(), &pt);
        views::View::ConvertPointFromScreen(
            owner_->GetWidget()->GetRootView(), &pt);
        tip_y = std::min(pt.y(), owner_->height() - kArrowMinOffset);
        tip_y = std::max(tip_y, kArrowMinOffset);
      }
      int top_base_y = tip_y - kArrowWidth / 2;
      int top_base_x, tip_x;
      if (arrow_location() == views::BubbleBorder::LEFT_BOTTOM) {
        top_base_x = inset.left() + kSystemTrayBubbleHorizontalInset;
        tip_x = top_base_x - kArrowHeight;
        arrow_reference.SetPoint(top_base_x + kArrowHeight, tip_y);
      } else {
        DCHECK(arrow_location() == views::BubbleBorder::RIGHT_BOTTOM);
        top_base_x = inset.left() + owner_->width() -
            kSystemTrayBubbleHorizontalInset;
        tip_x = top_base_x + kArrowHeight;
        arrow_reference.SetPoint(top_base_x - kArrowHeight, tip_y);
      }
      path.moveTo(SkIntToScalar(top_base_x), SkIntToScalar(top_base_y));
      path.lineTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
      path.lineTo(SkIntToScalar(top_base_x),
                  SkIntToScalar(top_base_y + kArrowWidth));
    }

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(background_color());
    canvas->DrawPath(path, paint);

    // Now draw the arrow border.
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(kBorderDarkColor);
    canvas->DrawPath(path, paint);

  }

  views::View* owner_;
  views::View* anchor_;
  const int tray_arrow_offset_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleBorder);
};

// Custom frame-view for the bubble. It overrides the following behaviour of the
// standard BubbleFrameView:
//   - Sets the minimum size to an empty box.
class TrayBubbleFrameView : public views::BubbleFrameView {
 public:
  TrayBubbleFrameView(const gfx::Insets& margins, TrayBubbleBorder* border)
      : views::BubbleFrameView(margins, border) {
  }

  virtual ~TrayBubbleFrameView() {}

  virtual gfx::Size GetMinimumSize() OVERRIDE {
    return gfx::Size();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayBubbleFrameView);
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

}  // namespace

namespace internal {

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
      arrow_offset(kArrowDefaultOffset),
      arrow_color(kHeaderBackgroundColorDark) {
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
      is_gesture_dragging_(false) {
  set_margins(gfx::Insets());
  set_parent_window(Shell::GetContainer(
      anchor->GetWidget()->GetNativeWindow()->GetRootWindow(),
      internal::kShellWindowId_SettingBubbleContainer));
  set_notify_enter_exit_on_child(true);
  set_close_on_deactivate(init_params.close_on_deactivate);
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(true);
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
  set_background(NULL);
}

gfx::Rect TrayBubbleView::GetAnchorRect() {
  gfx::Rect rect;

  if (anchor_widget()->IsVisible()) {
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

gfx::Rect TrayBubbleView::GetBubbleBounds() {
  // Same as BubbleDelegateView implementation, but don't try mirroring.
  gfx::Size use_size = is_gesture_dragging_ ? size() : GetPreferredSize();
  return GetBubbleFrameView()->GetUpdatedWindowBounds(
      GetAnchorRect(), use_size, false /*try_mirroring_arrow*/);
}

bool TrayBubbleView::CanActivate() const {
  return params_.can_activate;
}

// Overridden to create BubbleFrameView and set the border to TrayBubbleBorder
// (instead of creating a default BubbleBorder and replacing it).
views::NonClientFrameView* TrayBubbleView::CreateNonClientFrameView(
    views::Widget* widget) {
  TrayBubbleBorder* bubble_border = new TrayBubbleBorder(
      this, anchor_view(),
      arrow_location(), params_.arrow_offset, params_.arrow_color);
  return new TrayBubbleFrameView(margins(), bubble_border);
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
  tray_view_ = tray_view;

  // Must occur after call to BubbleDelegateView::CreateBubble().
  bubble_view->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);

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
  gfx::Rect bounds = widget_->GetNativeWindow()->GetBoundsInRootWindow();
  if (bounds.Contains(event.root_location()))
    return;
  if (tray_view_) {
    // If the user clicks on the parent tray, don't process the event here,
    // let the tray logic handle the event and determine show/hide behavior.
    bounds = tray_view_->ConvertRectToWidget(tray_view_->GetLocalBounds());
    if (bounds.Contains(event.location()))
      return;
  }
  // Handle clicking outside the bubble and tray. We don't block the event, so
  // it will also be handled by whatever widget was clicked on.
  OnClickedOutsideView();
}


}  // namespace internal
}  // namespace ash
