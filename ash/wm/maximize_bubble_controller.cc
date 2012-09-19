// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_bubble_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/workspace/frame_maximize_button.h"
#include "base/timer.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/window.h"
#include "ui/base/animation/animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/widget/widget.h"

namespace {

// The spacing between two buttons.
const int kLayoutSpacing = -1;

// The background color.
const SkColor kBubbleBackgroundColor = 0xc8141414;

// The text color within the bubble.
const SkColor kBubbleTextColor = SK_ColorWHITE;

// The line width of the bubble.
const int kLineWidth = 1;

// The spacing for the top and bottom of the info label.
const int kLabelSpacing = 4;

// The pixel dimensions of the arrow.
const int kArrowHeight = 10;
const int kArrowWidth = 20;

// The animation offset in y for the bubble when appearing.
const int kBubbleAnimationOffsetY = 5;

class MaximizeBubbleBorder : public views::BubbleBorder {
 public:
  MaximizeBubbleBorder(views::View* content_view, views::View* anchor);

  virtual ~MaximizeBubbleBorder() {}

  // Get the mouse active area of the window.
  void GetMask(gfx::Path* mask);

  // Overridden from views::BubbleBorder to match the design specs.
  virtual gfx::Rect GetBounds(const gfx::Rect& position_relative_to,
                              const gfx::Size& contents_size) const OVERRIDE;

  // Overridden from views::Border.
  virtual void Paint(const views::View& view,
                     gfx::Canvas* canvas) const OVERRIDE;

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
    : views::BubbleBorder(views::BubbleBorder::TOP_RIGHT,
                          views::BubbleBorder::NO_SHADOW),
      anchor_size_(anchor->size()),
      anchor_screen_origin_(0,0),
      content_view_(content_view) {
  views::View::ConvertPointToScreen(anchor, &anchor_screen_origin_);
  set_alignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
}

void MaximizeBubbleBorder::GetMask(gfx::Path* mask) {
  gfx::Insets inset;
  GetInsets(&inset);
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
  gfx::Insets insets;
  GetInsets(&insets);
  border_size.Enlarge(insets.width(), insets.height());

  // Position the bubble to center the box on the anchor.
  int x = (-border_size.width() + anchor_size_.width()) / 2;
  // Position the bubble under the anchor, overlapping the arrow with it.
  int y = anchor_size_.height() - insets.top();

  gfx::Point view_origin(x + anchor_screen_origin_.x(),
                         y + anchor_screen_origin_.y());

  return gfx::Rect(view_origin, border_size);
}

void MaximizeBubbleBorder::Paint(const views::View& view,
                                 gfx::Canvas* canvas) const {
  gfx::Insets inset;
  GetInsets(&inset);

  // Draw the border line around everything.
  int y = inset.top();
  // Top
  canvas->FillRect(gfx::Rect(inset.left(),
                             y - kLineWidth,
                             content_view_->width(),
                             kLineWidth),
                   kBubbleBackgroundColor);
  // Bottom
  canvas->FillRect(gfx::Rect(inset.left(),
                             y + content_view_->height(),
                             content_view_->width(),
                             kLineWidth),
                   kBubbleBackgroundColor);
  // Left
  canvas->FillRect(gfx::Rect(inset.left() - kLineWidth,
                             y - kLineWidth,
                             kLineWidth,
                             content_view_->height() + 2 * kLineWidth),
                   kBubbleBackgroundColor);
  // Right
  canvas->FillRect(gfx::Rect(inset.left() + content_view_->width(),
                             y - kLineWidth,
                             kLineWidth,
                             content_view_->height() + 2 * kLineWidth),
                   kBubbleBackgroundColor);

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
  paint.setColor(kBubbleBackgroundColor);
  canvas->DrawPath(path, paint);
}

}  // namespace

namespace ash {

class BubbleContentsButtonRow;
class BubbleContentsView;
class BubbleDialogButton;

// The mouse watcher host which makes sure that the bubble does not get closed
// while the mouse cursor is over the maximize button or the balloon content.
// Note: This object gets destroyed when the MouseWatcher gets destroyed.
class BubbleMouseWatcherHost: public views::MouseWatcherHost {
 public:
  explicit BubbleMouseWatcherHost(MaximizeBubbleController::Bubble* bubble)
      : bubble_(bubble) {}
  virtual ~BubbleMouseWatcherHost() {}

  // Implementation of MouseWatcherHost.
  virtual bool Contains(const gfx::Point& screen_point,
                        views::MouseWatcherHost::MouseEventType type);
 private:
  MaximizeBubbleController::Bubble* bubble_;

  DISALLOW_COPY_AND_ASSIGN(BubbleMouseWatcherHost);
};

// The class which creates and manages the bubble menu element.
// It creates a 'bubble border' and the content accordingly.
// Note: Since the SnapSizer will show animations on top of the maximize button
// this menu gets created as a separate window and the SnapSizer will be
// created underneath this window.
class MaximizeBubbleController::Bubble : public views::BubbleDelegateView,
                                         public views::MouseWatcherListener {
 public:
  explicit Bubble(MaximizeBubbleController* owner, int appearance_delay_ms_);
  virtual ~Bubble() {}

  // The window of the menu under which the SnapSizer will get created.
  aura::Window* GetBubbleWindow();

  // Overridden from views::BubbleDelegateView.
  virtual gfx::Rect GetAnchorRect() OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual bool CanActivate() const OVERRIDE { return false; }

  // Overridden from views::WidgetDelegateView.
  virtual bool HasHitTestMask() const OVERRIDE;
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE;

  // Implementation of MouseWatcherListener.
  virtual void MouseMovedOutOfHost();

  // Implementation of MouseWatcherHost.
  virtual bool Contains(const gfx::Point& screen_point,
                        views::MouseWatcherHost::MouseEventType type);

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::Widget::Observer.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // Called from the controller class to indicate that the menu should get
  // destroyed.
  virtual void ControllerRequestsCloseAndDelete();

  // Called from the owning class to change the menu content to the given
  // |snap_type| so that the user knows what is selected.
  void SetSnapType(SnapType snap_type);

  // Get the owning MaximizeBubbleController. This might return NULL in case
  // of an asynchronous shutdown.
  MaximizeBubbleController* controller() const { return owner_; }

  // Added for unit test: Retrieve the button for an action.
  // |state| can be either SNAP_LEFT, SNAP_RIGHT or SNAP_MINIMIZE.
  views::CustomButton* GetButtonForUnitTest(SnapType state);

 private:
  // True if the shut down has been initiated.
  bool shutting_down_;

  // Our owning class.
  MaximizeBubbleController* owner_;

  // The widget which contains our menu and the bubble border.
  views::Widget* bubble_widget_;

  // The content accessor of the menu.
  BubbleContentsView* contents_view_;

  // The bubble border.
  MaximizeBubbleBorder* bubble_border_;

  // The rectangle before the animation starts.
  gfx::Rect initial_position_;

  // The mouse watcher which takes care of out of window hover events.
  scoped_ptr<views::MouseWatcher> mouse_watcher_;

  // The fade delay - if 0 it will show / hide immediately.
  const int appearance_delay_ms_;

  DISALLOW_COPY_AND_ASSIGN(Bubble);
};

// A class that creates all buttons and put them into a view.
class BubbleContentsButtonRow : public views::View,
                                public views::ButtonListener {
 public:
  explicit BubbleContentsButtonRow(MaximizeBubbleController::Bubble* bubble);

  virtual ~BubbleContentsButtonRow() {}

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;
  // Called from BubbleDialogButton.
  void ButtonHovered(BubbleDialogButton* sender);

  // Added for unit test: Retrieve the button for an action.
  // |state| can be either SNAP_LEFT, SNAP_RIGHT or SNAP_MINIMIZE.
  views::CustomButton* GetButtonForUnitTest(SnapType state);

  MaximizeBubbleController::Bubble* bubble() { return bubble_; }

 private:
  // Functions to add the left and right maximize / restore buttons.
  void AddMaximizeLeftButton();
  void AddMaximizeRightButton();
  void AddMinimizeButton();

  // The owning object which gets notifications.
  MaximizeBubbleController::Bubble* bubble_;

  // The created buttons for our menu.
  BubbleDialogButton* left_button_;
  BubbleDialogButton* minimize_button_;
  BubbleDialogButton* right_button_;

  DISALLOW_COPY_AND_ASSIGN(BubbleContentsButtonRow);
};

// A class which creates the content of the bubble: The buttons, and the label.
class BubbleContentsView : public views::View {
 public:
  explicit BubbleContentsView(MaximizeBubbleController::Bubble* bubble);

  virtual ~BubbleContentsView() {}

  // Set the label content to reflect the currently selected |snap_type|.
  // This function can be executed through the frame maximize button as well as
  // through hover operations.
  void SetSnapType(SnapType snap_type);

  // Added for unit test: Retrieve the button for an action.
  // |state| can be either SNAP_LEFT, SNAP_RIGHT or SNAP_MINIMIZE.
  views::CustomButton* GetButtonForUnitTest(SnapType state) {
    return buttons_view_->GetButtonForUnitTest(state);
  }

 private:
  // The owning class.
  MaximizeBubbleController::Bubble* bubble_;

  // The object which owns all the buttons.
  BubbleContentsButtonRow* buttons_view_;

  // The label object which shows the user the selected action.
  views::Label* label_view_;

  DISALLOW_COPY_AND_ASSIGN(BubbleContentsView);
};

// The image button gets overridden to be able to capture mouse hover events.
// The constructor also assigns all button states and
class BubbleDialogButton : public views::ImageButton {
 public:
  explicit BubbleDialogButton(
      BubbleContentsButtonRow* button_row_listener,
      int normal_image,
      int hovered_image,
      int pressed_image);
  virtual ~BubbleDialogButton() {}

  // CustomButton overrides:
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;

 private:
  // The creating class which needs to get notified in case of a hover event.
  BubbleContentsButtonRow* button_row_;

  DISALLOW_COPY_AND_ASSIGN(BubbleDialogButton);
};

MaximizeBubbleController::Bubble::Bubble(
    MaximizeBubbleController* owner,
    int appearance_delay_ms)
    : views::BubbleDelegateView(owner->frame_maximize_button(),
                                views::BubbleBorder::TOP_RIGHT),
      shutting_down_(false),
      owner_(owner),
      bubble_widget_(NULL),
      contents_view_(NULL),
      bubble_border_(NULL),
      appearance_delay_ms_(appearance_delay_ms) {
  set_margins(gfx::Insets());

  // The window needs to be owned by the root so that the SnapSizer does not
  // cover it upon animation.
  aura::Window* parent = Shell::GetContainer(
      Shell::GetActiveRootWindow(),
      internal::kShellWindowId_LauncherContainer);
  set_parent_window(parent);

  set_notify_enter_exit_on_child(true);
  set_try_mirroring_arrow(false);
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(false);
  set_color(kBubbleBackgroundColor);
  set_close_on_deactivate(false);
  set_background(
      views::Background::CreateSolidBackground(kBubbleBackgroundColor));

  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, kLayoutSpacing));

  contents_view_ = new BubbleContentsView(this);
  AddChildView(contents_view_);

  // Note that the returned widget has an observer which points to our
  // functions.
  bubble_widget_ = views::BubbleDelegateView::CreateBubble(this);
  bubble_widget_->set_focus_on_creation(false);

  SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  bubble_widget_->non_client_view()->frame_view()->set_background(NULL);

  bubble_border_ = new MaximizeBubbleBorder(this, anchor_view());
  GetBubbleFrameView()->SetBubbleBorder(bubble_border_);
  GetBubbleFrameView()->set_background(NULL);

  // Recalculate size with new border.
  SizeToContents();

  if (!appearance_delay_ms_)
    Show();
  else
    StartFade(true);

  mouse_watcher_.reset(new views::MouseWatcher(
      new BubbleMouseWatcherHost(this),
      this));
  mouse_watcher_->Start();
}

bool BubbleMouseWatcherHost::Contains(
    const gfx::Point& screen_point,
    views::MouseWatcherHost::MouseEventType type) {
  return bubble_->Contains(screen_point, type);
}

aura::Window* MaximizeBubbleController::Bubble::GetBubbleWindow() {
  return bubble_widget_ ? bubble_widget_->GetNativeWindow() : NULL;
}

gfx::Rect MaximizeBubbleController::Bubble::GetAnchorRect() {
  if (!owner_)
    return gfx::Rect();

  gfx::Rect anchor_rect =
      owner_->frame_maximize_button()->GetBoundsInScreen();
  return anchor_rect;
}

void MaximizeBubbleController::Bubble::AnimationProgressed(
    const ui::Animation* animation) {
  // First do everything needed for the fade by calling the base function.
  BubbleDelegateView::AnimationProgressed(animation);
  // When fading in we are done.
  if (!shutting_down_)
    return;
  // Upon fade out an additional shift is required.
  int shift = animation->CurrentValueBetween(kBubbleAnimationOffsetY, 0);
  gfx::Rect rect = initial_position_;

  rect.set_y(rect.y() + shift);
  bubble_widget_->GetNativeWindow()->SetBounds(rect);
}

bool MaximizeBubbleController::Bubble::HasHitTestMask() const {
  return bubble_border_ != NULL;
}

void MaximizeBubbleController::Bubble::GetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  DCHECK(bubble_border_);
  bubble_border_->GetMask(mask);
}

void MaximizeBubbleController::Bubble::MouseMovedOutOfHost() {
  if (!owner_ || shutting_down_)
    return;
  // When we leave the bubble, we might be still be in gesture mode or over
  // the maximize button. So only close if none of the other cases apply.
  if (!owner_->frame_maximize_button()->is_snap_enabled()) {
    gfx::Point screen_location = gfx::Screen::GetCursorScreenPoint();
    if (!owner_->frame_maximize_button()->GetBoundsInScreen().Contains(
        screen_location)) {
        owner_->RequestDestructionThroughOwner();
    }
  }
}

bool MaximizeBubbleController::Bubble::Contains(
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

gfx::Size MaximizeBubbleController::Bubble::GetPreferredSize() {
  return contents_view_->GetPreferredSize();
}

void MaximizeBubbleController::Bubble::OnWidgetClosing(views::Widget* widget) {
  if (bubble_widget_ == widget) {
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
  BubbleDelegateView::OnWidgetClosing(widget);
}

void MaximizeBubbleController::Bubble::ControllerRequestsCloseAndDelete() {
  // This only gets called from the owning base class once it is deleted.
  if (shutting_down_)
    return;
  shutting_down_ = true;
  owner_ = NULL;

  // Close the widget asynchronously after the hide animation is finished.
  initial_position_ = bubble_widget_->GetNativeWindow()->bounds();
  if (!appearance_delay_ms_)
    bubble_widget_->CloseNow();
  else
    StartFade(false);
}

void MaximizeBubbleController::Bubble::SetSnapType(SnapType snap_type) {
  if (contents_view_)
    contents_view_->SetSnapType(snap_type);
}

views::CustomButton* MaximizeBubbleController::Bubble::GetButtonForUnitTest(
    SnapType state) {
  return contents_view_->GetButtonForUnitTest(state);
}

BubbleContentsButtonRow::BubbleContentsButtonRow(
    MaximizeBubbleController::Bubble* bubble)
    : bubble_(bubble),
      left_button_(NULL),
      minimize_button_(NULL),
      right_button_(NULL) {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kLayoutSpacing));
  set_background(
      views::Background::CreateSolidBackground(kBubbleBackgroundColor));

  if (base::i18n::IsRTL()) {
    AddMaximizeRightButton();
    AddMinimizeButton();
    AddMaximizeLeftButton();
  } else {
    AddMaximizeLeftButton();
    AddMinimizeButton();
    AddMaximizeRightButton();
  }
}

// Overridden from ButtonListener.
void BubbleContentsButtonRow::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  // While shutting down, the connection to the owner might already be broken.
  if (!bubble_->controller())
    return;
  if (sender == left_button_)
    bubble_->controller()->OnButtonClicked(
        bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_LEFT ?
            SNAP_RESTORE : SNAP_LEFT);
  else if (sender == minimize_button_)
    bubble_->controller()->OnButtonClicked(SNAP_MINIMIZE);
  else if (sender == right_button_)
    bubble_->controller()->OnButtonClicked(
        bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_RIGHT ?
            SNAP_RESTORE : SNAP_RIGHT);
  else
    NOTREACHED() << "Unknown button pressed.";
}

// Called from BubbleDialogButton.
void BubbleContentsButtonRow::ButtonHovered(BubbleDialogButton* sender) {
  // While shutting down, the connection to the owner might already be broken.
  if (!bubble_->controller())
    return;
  if (sender == left_button_)
    bubble_->controller()->OnButtonHover(
        bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_LEFT ?
            SNAP_RESTORE : SNAP_LEFT);
  else if (sender == minimize_button_)
    bubble_->controller()->OnButtonHover(SNAP_MINIMIZE);
  else if (sender == right_button_)
    bubble_->controller()->OnButtonHover(
        bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_RIGHT ?
            SNAP_RESTORE : SNAP_RIGHT);
  else
    bubble_->controller()->OnButtonHover(SNAP_NONE);
}

views::CustomButton* BubbleContentsButtonRow::GetButtonForUnitTest(
    SnapType state) {
  switch (state) {
    case SNAP_LEFT:
      return left_button_;
    case SNAP_MINIMIZE:
      return minimize_button_;
    case SNAP_RIGHT:
      return right_button_;
    default:
      NOTREACHED();
      return NULL;
  }
}

void BubbleContentsButtonRow::AddMaximizeLeftButton() {
  if (bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_LEFT) {
    left_button_ = new BubbleDialogButton(
        this,
        IDR_AURA_WINDOW_POSITION_LEFT_RESTORE,
        IDR_AURA_WINDOW_POSITION_LEFT_RESTORE_H,
        IDR_AURA_WINDOW_POSITION_LEFT_RESTORE_P);
  } else {
    left_button_ = new BubbleDialogButton(
        this,
        IDR_AURA_WINDOW_POSITION_LEFT,
        IDR_AURA_WINDOW_POSITION_LEFT_H,
        IDR_AURA_WINDOW_POSITION_LEFT_P);
  }
}

void BubbleContentsButtonRow::AddMaximizeRightButton() {
  if (bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_RIGHT) {
    right_button_ = new BubbleDialogButton(
        this,
        IDR_AURA_WINDOW_POSITION_RIGHT_RESTORE,
        IDR_AURA_WINDOW_POSITION_RIGHT_RESTORE_H,
        IDR_AURA_WINDOW_POSITION_RIGHT_RESTORE_P);
  } else {
    right_button_ = new BubbleDialogButton(
        this,
        IDR_AURA_WINDOW_POSITION_RIGHT,
        IDR_AURA_WINDOW_POSITION_RIGHT_H,
        IDR_AURA_WINDOW_POSITION_RIGHT_P);
  }
}

void BubbleContentsButtonRow::AddMinimizeButton() {
  minimize_button_ = new BubbleDialogButton(
      this,
      IDR_AURA_WINDOW_POSITION_MIDDLE,
      IDR_AURA_WINDOW_POSITION_MIDDLE_H,
      IDR_AURA_WINDOW_POSITION_MIDDLE_P);
}

BubbleContentsView::BubbleContentsView(
    MaximizeBubbleController::Bubble* bubble)
    : bubble_(bubble),
      buttons_view_(NULL),
      label_view_(NULL) {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, kLayoutSpacing));
  set_background(
      views::Background::CreateSolidBackground(kBubbleBackgroundColor));

  buttons_view_ = new BubbleContentsButtonRow(bubble);
  AddChildView(buttons_view_);

  label_view_ = new views::Label();
  SetSnapType(SNAP_NONE);
  label_view_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  label_view_->SetBackgroundColor(kBubbleBackgroundColor);
  label_view_->SetEnabledColor(kBubbleTextColor);
  label_view_->set_border(views::Border::CreateEmptyBorder(
      kLabelSpacing, 0, kLabelSpacing, 0));
  AddChildView(label_view_);
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

MaximizeBubbleController::MaximizeBubbleController(
    FrameMaximizeButton* frame_maximize_button,
    MaximizeBubbleFrameState maximize_type,
    int appearance_delay_ms)
    : frame_maximize_button_(frame_maximize_button),
      bubble_(NULL),
      maximize_type_(maximize_type),
      appearance_delay_ms_(appearance_delay_ms) {
  // Create the task which will create the bubble delayed.
  base::OneShotTimer<MaximizeBubbleController>* new_timer =
      new base::OneShotTimer<MaximizeBubbleController>();
  // Note: Even if there was no delay time given, we need to have a timer.
  new_timer->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(
          appearance_delay_ms_ ? appearance_delay_ms_ : 10),
      this,
      &MaximizeBubbleController::CreateBubble);
  timer_.reset(new_timer);
  if (!appearance_delay_ms_)
    CreateBubble();
}

MaximizeBubbleController::~MaximizeBubbleController() {
  // Note: The destructor only gets initiated through the owner.
  timer_.reset();
  if (bubble_) {
    bubble_->ControllerRequestsCloseAndDelete();
    bubble_ = NULL;
  }
}

void MaximizeBubbleController::SetSnapType(SnapType snap_type) {
  if (bubble_)
    bubble_->SetSnapType(snap_type);
}

aura::Window* MaximizeBubbleController::GetBubbleWindow() {
  return bubble_ ? bubble_->GetBubbleWindow() : NULL;
}

void MaximizeBubbleController::DelayCreation() {
  if (timer_.get() && timer_->IsRunning())
    timer_->Reset();
}

void MaximizeBubbleController::OnButtonClicked(SnapType snap_type) {
  frame_maximize_button_->ExecuteSnapAndCloseMenu(snap_type);
}

void MaximizeBubbleController::OnButtonHover(SnapType snap_type) {
  frame_maximize_button_->SnapButtonHovered(snap_type);
}

views::CustomButton* MaximizeBubbleController::GetButtonForUnitTest(
    SnapType state) {
  return bubble_ ? bubble_->GetButtonForUnitTest(state) : NULL;
}

void MaximizeBubbleController::RequestDestructionThroughOwner() {
  // Tell the parent to destroy us (if this didn't happen yet).
  if (timer_.get()) {
    timer_.reset(NULL);
    // Informs the owner that the menu is gone and requests |this| destruction.
    frame_maximize_button_->DestroyMaximizeMenu();
    // Note: After this call |this| is destroyed.
  }
}

void MaximizeBubbleController::CreateBubble() {
  if (!bubble_)
    bubble_ = new Bubble(this, appearance_delay_ms_);

  timer_->Stop();
}

BubbleDialogButton::BubbleDialogButton(
    BubbleContentsButtonRow* button_row,
    int normal_image,
    int hovered_image,
    int pressed_image)
    : views::ImageButton(button_row),
      button_row_(button_row) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetImage(views::CustomButton::BS_NORMAL, rb.GetImageSkiaNamed(normal_image));
  SetImage(views::CustomButton::BS_HOT, rb.GetImageSkiaNamed(hovered_image));
  SetImage(views::CustomButton::BS_PUSHED,
           rb.GetImageSkiaNamed(pressed_image));
  button_row->AddChildView(this);
}

void BubbleDialogButton::OnMouseCaptureLost() {
  button_row_->ButtonHovered(NULL);
  views::ImageButton::OnMouseCaptureLost();
}

void BubbleDialogButton::OnMouseEntered(const ui::MouseEvent& event) {
  button_row_->ButtonHovered(this);
  views::ImageButton::OnMouseEntered(event);
}

void BubbleDialogButton::OnMouseExited(const ui::MouseEvent& event) {
  button_row_->ButtonHovered(NULL);
  views::ImageButton::OnMouseExited(event);
}

bool BubbleDialogButton::OnMouseDragged(const ui::MouseEvent& event) {
  if (!button_row_->bubble()->controller())
    return false;

  // Remove the phantom window when we leave the button.
  gfx::Point screen_location(event.location());
  View::ConvertPointToScreen(this, &screen_location);
  if (!GetBoundsInScreen().Contains(screen_location))
    button_row_->ButtonHovered(NULL);
  else
    button_row_->ButtonHovered(this);

  // Pass the event on to the normal handler.
  return views::ImageButton::OnMouseDragged(event);
}

}  // namespace ash
