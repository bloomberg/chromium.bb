// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/caption_buttons/maximize_bubble_controller_bubble.h"

#include "ash/frame/caption_buttons/bubble_contents_button_row.h"
#include "ash/frame/caption_buttons/frame_maximize_button.h"
#include "ash/frame/caption_buttons/maximize_bubble_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/mouse_watcher.h"
#include "ui/wm/core/masked_window_targeter.h"

namespace ash {

// BubbleContentsView ---------------------------------------------------------

// A class which creates the content of the bubble: The buttons, and the label.
class BubbleContentsView : public views::View {
 public:
  BubbleContentsView(MaximizeBubbleControllerBubble* bubble,
                     SnapType initial_snap_type);
  virtual ~BubbleContentsView();

  // Set the label content to reflect the currently selected |snap_type|.
  // This function can be executed through the frame maximize button as well as
  // through hover operations.
  void SetSnapType(SnapType snap_type);

  // Added for unit test: Retrieve the button for an action.
  // |state| can be either SNAP_LEFT, SNAP_RIGHT or SNAP_MINIMIZE.
  views::CustomButton* GetButtonForUnitTest(SnapType state);

 private:
  // The owning class.
  MaximizeBubbleControllerBubble* bubble_;

  // The object which owns all the buttons.
  BubbleContentsButtonRow* buttons_view_;

  // The label object which shows the user the selected action.
  views::Label* label_view_;

  DISALLOW_COPY_AND_ASSIGN(BubbleContentsView);
};

BubbleContentsView::BubbleContentsView(
    MaximizeBubbleControllerBubble* bubble,
    SnapType initial_snap_type)
    : bubble_(bubble),
      buttons_view_(NULL),
      label_view_(NULL) {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0,
      MaximizeBubbleControllerBubble::kLayoutSpacing));
  set_background(views::Background::CreateSolidBackground(
      MaximizeBubbleControllerBubble::kBubbleBackgroundColor));

  buttons_view_ = new BubbleContentsButtonRow(bubble);
  AddChildView(buttons_view_);

  label_view_ = new views::Label();
  SetSnapType(initial_snap_type);
  label_view_->SetBackgroundColor(
      MaximizeBubbleControllerBubble::kBubbleBackgroundColor);
  const SkColor kBubbleTextColor = SK_ColorWHITE;
  label_view_->SetEnabledColor(kBubbleTextColor);
  const int kLabelSpacing = 4;
  label_view_->SetBorder(
      views::Border::CreateEmptyBorder(kLabelSpacing, 0, kLabelSpacing, 0));
  AddChildView(label_view_);
}

BubbleContentsView::~BubbleContentsView() {
}

// Set the label content to reflect the currently selected |snap_type|.
// This function can be executed through the frame maximize button as well as
// through hover operations.
void BubbleContentsView::SetSnapType(SnapType snap_type) {
  if (!bubble_->controller())
    return;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  int id = 0;
  switch (snap_type) {
    case SNAP_LEFT:
      id = IDS_ASH_SNAP_WINDOW_LEFT;
      break;
    case SNAP_RIGHT:
      id = IDS_ASH_SNAP_WINDOW_RIGHT;
      break;
    case SNAP_MAXIMIZE:
      DCHECK_NE(FRAME_STATE_FULL, bubble_->controller()->maximize_type());
      id = IDS_ASH_MAXIMIZE_WINDOW;
      break;
    case SNAP_MINIMIZE:
      id = IDS_ASH_MINIMIZE_WINDOW;
      break;
    case SNAP_RESTORE:
      DCHECK_NE(FRAME_STATE_NONE, bubble_->controller()->maximize_type());
      id = IDS_ASH_RESTORE_WINDOW;
      break;
    default:
      // If nothing is selected, we automatically select the click operation.
      id = bubble_->controller()->maximize_type() == FRAME_STATE_FULL ?
               IDS_ASH_RESTORE_WINDOW : IDS_ASH_MAXIMIZE_WINDOW;
      break;
  }
  label_view_->SetText(rb.GetLocalizedString(id));
}

views::CustomButton* BubbleContentsView::GetButtonForUnitTest(SnapType state) {
  return buttons_view_->GetButtonForUnitTest(state);
}


// MaximizeBubbleBorder -------------------------------------------------------

namespace {

const int kLineWidth = 1;
const int kArrowHeight = 10;
const int kArrowWidth = 20;

}  // namespace

class MaximizeBubbleBorder : public views::BubbleBorder {
 public:
  MaximizeBubbleBorder(views::View* content_view, views::View* anchor);

  virtual ~MaximizeBubbleBorder() {}

  // Get the mouse active area of the window.
  void GetMask(gfx::Path* mask);

  // views::BubbleBorder:
  virtual gfx::Rect GetBounds(const gfx::Rect& position_relative_to,
                              const gfx::Size& contents_size) const OVERRIDE;
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;

 private:
  // Note: Animations can continue after then main window frame was destroyed.
  // To avoid this problem, the owning screen metrics get extracted upon
  // creation.
  gfx::Size anchor_size_;
  gfx::Point anchor_screen_origin_;
  views::View* content_view_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeBubbleBorder);
};

MaximizeBubbleBorder::MaximizeBubbleBorder(views::View* content_view,
                                           views::View* anchor)
    : views::BubbleBorder(
          views::BubbleBorder::TOP_RIGHT, views::BubbleBorder::NO_SHADOW,
          MaximizeBubbleControllerBubble::kBubbleBackgroundColor),
      anchor_size_(anchor->size()),
      anchor_screen_origin_(0, 0),
      content_view_(content_view) {
  views::View::ConvertPointToScreen(anchor, &anchor_screen_origin_);
  set_alignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
}

void MaximizeBubbleBorder::GetMask(gfx::Path* mask) {
  gfx::Insets inset = GetInsets();
  // Note: Even though the tip could be added as activatable, it is left out
  // since it would not change the action behavior in any way plus it makes
  // more sense to keep the focus on the underlying button for clicks.
  int left = inset.left() - kLineWidth;
  int right = inset.left() + content_view_->width() + kLineWidth;
  int top = inset.top() - kLineWidth;
  int bottom = inset.top() + content_view_->height() + kLineWidth;
  mask->moveTo(left, top);
  mask->lineTo(right, top);
  mask->lineTo(right, bottom);
  mask->lineTo(left, bottom);
  mask->lineTo(left, top);
  mask->close();
}

gfx::Rect MaximizeBubbleBorder::GetBounds(
    const gfx::Rect& position_relative_to,
    const gfx::Size& contents_size) const {
  gfx::Size border_size(contents_size);
  gfx::Insets insets = GetInsets();
  border_size.Enlarge(insets.width(), insets.height());

  // Position the bubble to center the box on the anchor.
  int x = (anchor_size_.width() - border_size.width()) / 2;
  // Position the bubble under the anchor, overlapping the arrow with it.
  int y = anchor_size_.height() - insets.top();

  gfx::Point view_origin(x + anchor_screen_origin_.x(),
                         y + anchor_screen_origin_.y());

  return gfx::Rect(view_origin, border_size);
}

void MaximizeBubbleBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  gfx::Insets inset = GetInsets();

  // Draw the border line around everything.
  int y = inset.top();
  // Top
  canvas->FillRect(gfx::Rect(inset.left(),
                             y - kLineWidth,
                             content_view_->width(),
                             kLineWidth),
                   MaximizeBubbleControllerBubble::kBubbleBackgroundColor);
  // Bottom
  canvas->FillRect(gfx::Rect(inset.left(),
                             y + content_view_->height(),
                             content_view_->width(),
                             kLineWidth),
                   MaximizeBubbleControllerBubble::kBubbleBackgroundColor);
  // Left
  canvas->FillRect(gfx::Rect(inset.left() - kLineWidth,
                             y - kLineWidth,
                             kLineWidth,
                             content_view_->height() + 2 * kLineWidth),
                   MaximizeBubbleControllerBubble::kBubbleBackgroundColor);
  // Right
  canvas->FillRect(gfx::Rect(inset.left() + content_view_->width(),
                             y - kLineWidth,
                             kLineWidth,
                             content_view_->height() + 2 * kLineWidth),
                   MaximizeBubbleControllerBubble::kBubbleBackgroundColor);

  // Draw the arrow afterwards covering the border.
  SkPath path;
  path.incReserve(4);
  // The center of the tip should be in the middle of the button.
  int tip_x = inset.left() + content_view_->width() / 2;
  int left_base_x = tip_x - kArrowWidth / 2;
  int left_base_y = y;
  int tip_y = left_base_y - kArrowHeight;
  path.moveTo(SkIntToScalar(left_base_x), SkIntToScalar(left_base_y));
  path.lineTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
  path.lineTo(SkIntToScalar(left_base_x + kArrowWidth),
              SkIntToScalar(left_base_y));

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(MaximizeBubbleControllerBubble::kBubbleBackgroundColor);
  canvas->DrawPath(path, paint);
}

gfx::Size MaximizeBubbleBorder::GetMinimumSize() const {
  return gfx::Size(kLineWidth * 2 + kArrowWidth,
                   std::max(kLineWidth, kArrowHeight) + kLineWidth);
}

namespace {

// MaximizebubbleTargeter  -----------------------------------------------------

// Window targeter used for the bubble.
class MaximizeBubbleTargeter : public ::wm::MaskedWindowTargeter {
 public:
  MaximizeBubbleTargeter(aura::Window* window,
                         MaximizeBubbleBorder* border)
      : ::wm::MaskedWindowTargeter(window),
        border_(border) {
  }

  virtual ~MaximizeBubbleTargeter() {}

 private:
  // ::wm::MaskedWindowTargeter:
  virtual bool GetHitTestMask(aura::Window* window,
                              gfx::Path* mask) const OVERRIDE {
    border_->GetMask(mask);
    return true;
  }

  MaximizeBubbleBorder* border_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeBubbleTargeter);
};

}  // namespace


// BubbleMouseWatcherHost -----------------------------------------------------

// The mouse watcher host which makes sure that the bubble does not get closed
// while the mouse cursor is over the maximize button or the balloon content.
// Note: This object gets destroyed when the MouseWatcher gets destroyed.
class BubbleMouseWatcherHost: public views::MouseWatcherHost {
 public:
  explicit BubbleMouseWatcherHost(MaximizeBubbleControllerBubble* bubble);
  virtual ~BubbleMouseWatcherHost();

  // views::MouseWatcherHost:
  virtual bool Contains(const gfx::Point& screen_point,
                        views::MouseWatcherHost::MouseEventType type) OVERRIDE;
 private:
  MaximizeBubbleControllerBubble* bubble_;

  DISALLOW_COPY_AND_ASSIGN(BubbleMouseWatcherHost);
};

BubbleMouseWatcherHost::BubbleMouseWatcherHost(
    MaximizeBubbleControllerBubble* bubble)
    : bubble_(bubble) {
}

BubbleMouseWatcherHost::~BubbleMouseWatcherHost() {
}

bool BubbleMouseWatcherHost::Contains(
    const gfx::Point& screen_point,
    views::MouseWatcherHost::MouseEventType type) {
  return bubble_->Contains(screen_point, type);
}


// MaximizeBubbleControllerBubble ---------------------------------------------

// static
const SkColor MaximizeBubbleControllerBubble::kBubbleBackgroundColor =
    0xFF141414;
const int MaximizeBubbleControllerBubble::kLayoutSpacing = -1;

MaximizeBubbleControllerBubble::MaximizeBubbleControllerBubble(
    MaximizeBubbleController* owner,
    int appearance_delay_ms,
    SnapType initial_snap_type)
    : views::BubbleDelegateView(owner->frame_maximize_button(),
                                views::BubbleBorder::TOP_RIGHT),
      shutting_down_(false),
      owner_(owner),
      contents_view_(NULL),
      bubble_border_(NULL),
      appearance_delay_ms_(appearance_delay_ms) {
  set_margins(gfx::Insets());

  // The window needs to be owned by the root so that the phantom window does
  // not cover it upon animation.
  aura::Window* parent = Shell::GetContainer(Shell::GetTargetRootWindow(),
                                             kShellWindowId_ShelfContainer);
  set_parent_window(parent);

  set_notify_enter_exit_on_child(true);
  set_adjust_if_offscreen(false);
  SetPaintToLayer(true);
  set_color(kBubbleBackgroundColor);
  set_close_on_deactivate(false);
  set_background(
      views::Background::CreateSolidBackground(kBubbleBackgroundColor));

  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, kLayoutSpacing));

  contents_view_ = new BubbleContentsView(this, initial_snap_type);
  AddChildView(contents_view_);

  // Note that the returned widget has an observer which points to our
  // functions.
  views::Widget* bubble_widget = views::BubbleDelegateView::CreateBubble(this);
  bubble_widget->set_focus_on_creation(false);

  SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  bubble_widget->non_client_view()->frame_view()->set_background(NULL);

  bubble_border_ = new MaximizeBubbleBorder(this, GetAnchorView());
  GetBubbleFrameView()->SetBubbleBorder(
      scoped_ptr<views::BubbleBorder>(bubble_border_));
  GetBubbleFrameView()->set_background(NULL);

  // Recalculate size with new border.
  SizeToContents();

  GetWidget()->Show();

  aura::Window* window = bubble_widget->GetNativeWindow();
  window->SetEventTargeter(scoped_ptr<ui::EventTargeter>(
      new MaximizeBubbleTargeter(window, bubble_border_)));

  ash::Shell::GetInstance()->metrics()->RecordUserMetricsAction(
      ash::UMA_WINDOW_MAXIMIZE_BUTTON_SHOW_BUBBLE);

  mouse_watcher_.reset(new views::MouseWatcher(
      new BubbleMouseWatcherHost(this),
      this));
  mouse_watcher_->Start();
}

MaximizeBubbleControllerBubble::~MaximizeBubbleControllerBubble() {
}

aura::Window* MaximizeBubbleControllerBubble::GetBubbleWindow() {
  return GetWidget()->GetNativeWindow();
}

gfx::Rect MaximizeBubbleControllerBubble::GetAnchorRect() {
  if (!owner_)
    return gfx::Rect();

  gfx::Rect anchor_rect =
      owner_->frame_maximize_button()->GetBoundsInScreen();
  return anchor_rect;
}

bool MaximizeBubbleControllerBubble::CanActivate() const {
  return false;
}

bool MaximizeBubbleControllerBubble::WidgetHasHitTestMask() const {
  return bubble_border_ != NULL;
}

void MaximizeBubbleControllerBubble::GetWidgetHitTestMask(
    gfx::Path* mask) const {
  DCHECK(mask);
  DCHECK(bubble_border_);
  bubble_border_->GetMask(mask);
}

void MaximizeBubbleControllerBubble::MouseMovedOutOfHost() {
  if (!owner_ || shutting_down_)
    return;
  // When we leave the bubble, we might be still be in gesture mode or over
  // the maximize button. So only close if none of the other cases apply.
  if (!owner_->frame_maximize_button()->is_snap_enabled()) {
    gfx::Point screen_location = Shell::GetScreen()->GetCursorScreenPoint();
    if (!owner_->frame_maximize_button()->GetBoundsInScreen().Contains(
        screen_location)) {
      owner_->RequestDestructionThroughOwner();
    }
  }
}

bool MaximizeBubbleControllerBubble::Contains(
    const gfx::Point& screen_point,
    views::MouseWatcherHost::MouseEventType type) {
  if (!owner_ || shutting_down_)
    return false;
  bool inside_button =
      owner_->frame_maximize_button()->GetBoundsInScreen().Contains(
          screen_point);
  if (!owner_->frame_maximize_button()->is_snap_enabled() && inside_button) {
    SetSnapType(controller()->maximize_type() == FRAME_STATE_FULL ?
        SNAP_RESTORE : SNAP_MAXIMIZE);
    return true;
  }
  // Check if either a gesture is taking place (=> bubble stays no matter what
  // the mouse does) or the mouse is over the maximize button or the bubble
  // content.
  return (owner_->frame_maximize_button()->is_snap_enabled() ||
          inside_button ||
          contents_view_->GetBoundsInScreen().Contains(screen_point));
}

gfx::Size MaximizeBubbleControllerBubble::GetPreferredSize() {
  return contents_view_->GetPreferredSize();
}

void MaximizeBubbleControllerBubble::OnWidgetDestroying(views::Widget* widget) {
  if (GetWidget() == widget) {
    mouse_watcher_->Stop();

    if (owner_) {
      // If the bubble destruction was triggered by some other external
      // influence then ourselves, the owner needs to be informed that the menu
      // is gone.
      shutting_down_ = true;
      owner_->RequestDestructionThroughOwner();
      owner_ = NULL;
    }
  }
  BubbleDelegateView::OnWidgetDestroying(widget);
}

void MaximizeBubbleControllerBubble::ControllerRequestsCloseAndDelete() {
  // This only gets called from the owning base class once it is deleted.
  if (shutting_down_)
    return;
  shutting_down_ = true;
  owner_ = NULL;

  // Close the widget asynchronously after the hide animation is finished.
  if (!appearance_delay_ms_)
    GetWidget()->CloseNow();
  else
    GetWidget()->Close();
}

void MaximizeBubbleControllerBubble::SetSnapType(SnapType snap_type) {
  if (contents_view_)
    contents_view_->SetSnapType(snap_type);
}

views::CustomButton* MaximizeBubbleControllerBubble::GetButtonForUnitTest(
    SnapType state) {
  return contents_view_->GetButtonForUnitTest(state);
}

}  // namespace ash
