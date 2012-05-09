// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray.h"

#include "ash/shell.h"
#include "ash/shell/panel_window.h"
#include "ash/shell_window_ids.h"
#include "ash/system/audio/tray_volume.h"
#include "ash/system/bluetooth/tray_bluetooth.h"
#include "ash/system/brightness/tray_brightness.h"
#include "ash/system/date/tray_date.h"
#include "ash/system/drive/tray_drive.h"
#include "ash/system/ime/tray_ime.h"
#include "ash/system/network/tray_network.h"
#include "ash/system/power/power_status_observer.h"
#include "ash/system/power/power_supply_status.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/settings/tray_settings.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/system_tray_widget_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_empty.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/tray_caps_lock.h"
#include "ash/system/tray_update.h"
#include "ash/system/user/login_status.h"
#include "ash/system/user/tray_user.h"
#include "ash/wm/shadow_types.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_pump_observer.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/aura/root_window.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/events.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const int kPaddingFromRightEdgeOfScreen = 15;
const int kPaddingFromBottomOfScreen = 10;

const int kAnimationDurationForPopupMS = 200;

const int kArrowHeight = 10;
const int kArrowWidth = 20;
const int kArrowPaddingFromRight = 20;

const int kShadowThickness = 4;

const int kLeftPadding = 4;
const int kBottomLineHeight = 1;

const SkColor kShadowColor = SkColorSetARGB(0xff, 0, 0, 0);

const SkColor kTrayBackgroundAlpha = 100;
const SkColor kTrayBackgroundHoverAlpha = 150;

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

// A view with some special behaviour for tray items in the popup:
// - changes background color on hover.
class TrayPopupItemContainer : public views::View {
 public:
  explicit TrayPopupItemContainer(views::View* view) : hover_(false) {
    set_notify_enter_exit_on_child(true);
    set_border(view->border() ? views::Border::CreateEmptyBorder(0, 0, 0, 0) :
                                NULL);
    SetLayoutManager(new views::FillLayout);
    SetPaintToLayer(view->layer() != NULL);
    if (view->layer())
      SetFillsBoundsOpaquely(view->layer()->fills_bounds_opaquely());
    AddChildView(view);
    SetVisible(view->visible());
  }

  virtual ~TrayPopupItemContainer() {}

 private:
  // Overridden from views::View.
  virtual void ChildVisibilityChanged(View* child) OVERRIDE {
    if (visible() == child->visible())
      return;
    SetVisible(child->visible());
    PreferredSizeChanged();
  }

  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE {
    PreferredSizeChanged();
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    hover_ = true;
    SchedulePaint();
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    hover_ = false;
    SchedulePaint();
  }

  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE {
    if (child_count() == 0)
      return;

    views::View* view = child_at(0);
    if (!view->background()) {
      canvas->FillRect(gfx::Rect(size()),
          hover_ ? kHoverBackgroundColor : kBackgroundColor);
    }
  }

  bool hover_;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupItemContainer);
};

class SystemTrayBubbleBackground : public views::Background {
 public:
  explicit SystemTrayBubbleBackground(views::View* owner)
      : owner_(owner) {
  }

  virtual ~SystemTrayBubbleBackground() {}

 private:
  // Overridden from views::Background.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    views::View* last_view = NULL;
    for (int i = 0; i < owner_->child_count(); i++) {
      views::View* v = owner_->child_at(i);

      if (!v->border()) {
        canvas->DrawLine(gfx::Point(v->x(), v->y() - 1),
            gfx::Point(v->x() + v->width(), v->y() - 1),
            !last_view || last_view->border() ? kBorderDarkColor :
                                                kBorderLightColor);
        canvas->DrawLine(gfx::Point(v->x() - 1, v->y() - 1),
            gfx::Point(v->x() - 1, v->y() + v->height() + 1),
            kBorderDarkColor);
        canvas->DrawLine(gfx::Point(v->x() + v->width(), v->y() - 1),
            gfx::Point(v->x() + v->width(), v->y() + v->height() + 1),
            kBorderDarkColor);
      } else if (last_view && !last_view->border()) {
        canvas->DrawLine(gfx::Point(v->x() - 1, v->y() - 1),
            gfx::Point(v->x() + v->width() + 1, v->y() - 1),
            kBorderDarkColor);
      }

      last_view = v;
    }
  }

  views::View* owner_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubbleBackground);
};

class SystemTrayBubbleBorder : public views::BubbleBorder {
 public:
  enum ArrowType {
    ARROW_TYPE_NONE,
    ARROW_TYPE_BOTTOM,
  };

  SystemTrayBubbleBorder(views::View* owner, ArrowType arrow_type)
      : views::BubbleBorder(views::BubbleBorder::BOTTOM_RIGHT,
                            views::BubbleBorder::NO_SHADOW),
        owner_(owner),
        arrow_type_(arrow_type) {
    set_alignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  }

  virtual ~SystemTrayBubbleBorder() {}

 private:
  // Overridden from views::Border.
  virtual void Paint(const views::View& view,
                     gfx::Canvas* canvas) const OVERRIDE {
    views::View* first = NULL, *last = NULL;
    gfx::Insets inset;
    GetInsets(&inset);
    for (int i = 0; i < owner_->child_count(); i++) {
      views::View* v = owner_->child_at(i);
      if (v->border()) {
        if (first) {
          DrawBlurredShadowAroundView(canvas, first->y(),
              last->y() + last->height(), owner_->width(), inset);
          first = NULL;
          last = NULL;
        }
        continue;
      }

      if (!first)
        first = v;
      last = v;
    }
    if (first) {
      DrawBlurredShadowAroundView(canvas, first->y(),
          last->y() + last->height(), owner_->width(), inset);
    }

    // Draw the bottom line.
    int y = owner_->height() + 1;
    canvas->FillRect(gfx::Rect(kLeftPadding, y, owner_->width(),
                               kBottomLineHeight), kBorderDarkColor);

    if (!Shell::GetInstance()->shelf()->IsVisible())
      return;

    // Draw the arrow.
    if (arrow_type_ == ARROW_TYPE_BOTTOM) {
      int left_base_x = base::i18n::IsRTL() ? kArrowWidth :
          owner_->width() - kArrowPaddingFromRight - kArrowWidth;
      int left_base_y = y;
      int tip_x = left_base_x + kArrowWidth / 2;
      int tip_y = left_base_y + kArrowHeight;
      SkPath path;
      path.incReserve(4);
      path.moveTo(SkIntToScalar(left_base_x), SkIntToScalar(left_base_y));
      path.lineTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
      path.lineTo(SkIntToScalar(left_base_x + kArrowWidth),
                  SkIntToScalar(left_base_y));

      SkPaint paint;
      paint.setStyle(SkPaint::kFill_Style);
      paint.setColor(kBackgroundColor);
      canvas->DrawPath(path, paint);

      // Now draw the arrow border.
      paint.setStyle(SkPaint::kStroke_Style);
      paint.setColor(kBorderDarkColor);
      canvas->DrawPath(path, paint);
    }
  }

  views::View* owner_;
  ArrowType arrow_type_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubbleBorder);
};

}  // namespace

namespace internal {

class SystemTrayBackground : public views::Background {
 public:
  SystemTrayBackground() : alpha_(kTrayBackgroundAlpha) {}
  virtual ~SystemTrayBackground() {}

  void set_alpha(int alpha) { alpha_ = alpha; }

 private:
  // Overridden from views::Background.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SkColorSetARGB(alpha_, 0, 0, 0));
    SkPath path;
    gfx::Rect bounds(view->bounds());
    SkScalar radius = SkIntToScalar(kTrayRoundedBorderRadius);
    path.addRoundRect(gfx::RectToSkRect(bounds), radius, radius);
    canvas->DrawPath(path, paint);
  }

  int alpha_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBackground);
};

class SystemTrayBubble;

class SystemTrayBubbleView : public views::BubbleDelegateView {
 public:
  SystemTrayBubbleView(views::View* anchor,
                       SystemTrayBubble* host,
                       bool can_activate);
  virtual ~SystemTrayBubbleView();

  void SetBubbleBorder(views::BubbleBorder* border) {
    GetBubbleFrameView()->SetBubbleBorder(border);
  }

  void UpdateAnchor() {
    SizeToContents();
    GetWidget()->GetRootView()->SchedulePaint();
  }

  // Called when the host is destroyed.
  void reset_host() { host_ = NULL; }

 private:
  // Overridden from views::BubbleDelegateView.
  virtual void Init() OVERRIDE;
  virtual gfx::Rect GetAnchorRect() OVERRIDE;
  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool CanActivate() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;

  SystemTrayBubble* host_;
  bool can_activate_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubbleView);
};

class SystemTrayBubble : public base::MessagePumpObserver,
                         public views::Widget::Observer {
 public:
  enum BubbleType {
    BUBBLE_TYPE_DEFAULT,
    BUBBLE_TYPE_DETAILED,
    BUBBLE_TYPE_NOTIFICATION
  };

  enum AnchorType {
    ANCHOR_TYPE_TRAY,
    ANCHOR_TYPE_BUBBLE
  };

  SystemTrayBubble(ash::SystemTray* tray,
                   const std::vector<ash::SystemTrayItem*>& items,
                   BubbleType bubble_type);
  virtual ~SystemTrayBubble();

  // Creates |bubble_view_| and a child views for each member of |items_|.
  // Also creates |bubble_widget_| and sets up animations.
  void InitView(views::View* anchor,
                AnchorType anchor_type,
                bool can_activate,
                ash::user::LoginStatus login_status);

  gfx::Rect GetAnchorRect() const;

  BubbleType bubble_type() const { return bubble_type_; }
  SystemTrayBubbleView* bubble_view() const { return bubble_view_; }

  void DestroyItemViews();
  void StartAutoCloseTimer(int seconds);
  void StopAutoCloseTimer();
  void RestartAutoCloseTimer();
  void Close();

 private:
  // Overridden from base::MessagePumpObserver.
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE;
  // Overridden from views::Widget::Observer.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetVisibilityChanged(views::Widget* widget,
                                         bool visible) OVERRIDE;

  ash::SystemTray* tray_;
  SystemTrayBubbleView* bubble_view_;
  views::Widget* bubble_widget_;
  std::vector<ash::SystemTrayItem*> items_;
  BubbleType bubble_type_;
  AnchorType anchor_type_;

  int autoclose_delay_;
  base::OneShotTimer<SystemTrayBubble> autoclose_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubble);
};

// SystemTrayBubbleView

SystemTrayBubbleView::SystemTrayBubbleView(views::View* anchor,
                                           SystemTrayBubble* host,
                                           bool can_activate)
    : views::BubbleDelegateView(anchor, views::BubbleBorder::BOTTOM_RIGHT),
      host_(host),
      can_activate_(can_activate) {
  set_margin(0);
  set_parent_window(ash::Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_SettingBubbleContainer));
  set_notify_enter_exit_on_child(true);
}

SystemTrayBubbleView::~SystemTrayBubbleView() {
  // Inform host items (models) that their views are being destroyed.
  if (host_)
    host_->DestroyItemViews();
}

void SystemTrayBubbleView::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 1, 1, 1));
  set_background(new SystemTrayBubbleBackground(this));
}

gfx::Rect SystemTrayBubbleView::GetAnchorRect() {
  gfx::Rect rect;
  if (host_)
    rect = host_->GetAnchorRect();
  if (rect.IsEmpty()) {
    rect = gfx::Screen::GetPrimaryMonitor().bounds();
    rect = gfx::Rect(base::i18n::IsRTL() ? kPaddingFromRightEdgeOfScreen :
                     rect.width() - kPaddingFromRightEdgeOfScreen,
                     rect.height() - kPaddingFromBottomOfScreen,
                     0, 0);
  }
  return rect;
}

void SystemTrayBubbleView::ChildPreferredSizeChanged(View* child) {
  SizeToContents();
}

void SystemTrayBubbleView::GetAccessibleState(ui::AccessibleViewState* state) {
  if (can_activate_) {
    state->role = ui::AccessibilityTypes::ROLE_WINDOW;
    state->name = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ACCESSIBLE_NAME);
  }
}

bool SystemTrayBubbleView::CanActivate() const {
  return can_activate_;
}

gfx::Size SystemTrayBubbleView::GetPreferredSize() {
  gfx::Size size = views::BubbleDelegateView::GetPreferredSize();
  return gfx::Size(kTrayPopupWidth, size.height());
}

void SystemTrayBubbleView::OnMouseEntered(const views::MouseEvent& event) {
  if (host_)
    host_->StopAutoCloseTimer();
}

void SystemTrayBubbleView::OnMouseExited(const views::MouseEvent& event) {
  if (host_)
    host_->RestartAutoCloseTimer();
}

// SystemTrayBubble

SystemTrayBubble::SystemTrayBubble(
    ash::SystemTray* tray,
    const std::vector<ash::SystemTrayItem*>& items,
    BubbleType bubble_type)
    : tray_(tray),
      bubble_view_(NULL),
      bubble_widget_(NULL),
      items_(items),
      bubble_type_(bubble_type),
      anchor_type_(ANCHOR_TYPE_TRAY),
      autoclose_delay_(0) {
}

SystemTrayBubble::~SystemTrayBubble() {
  // The bubble may be closing without having been hidden first. So it may still
  // be in the message-loop's observer list.
  MessageLoopForUI::current()->RemoveObserver(this);

  DestroyItemViews();
  // Reset the host pointer in bubble_view_ in case its destruction is deferred.
  if (bubble_view_)
    bubble_view_->reset_host();
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    // This triggers the destruction of bubble_view_.
    bubble_widget_->Close();
  }
}

void SystemTrayBubble::InitView(views::View* anchor,
                                AnchorType anchor_type,
                                bool can_activate,
                                ash::user::LoginStatus login_status) {
  DCHECK(bubble_view_ == NULL);
  anchor_type_ = anchor_type;
  bubble_view_ = new SystemTrayBubbleView(anchor, this, can_activate);

  for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
       it != items_.end();
       ++it) {
    views::View* view = NULL;
    switch (bubble_type_) {
      case BUBBLE_TYPE_DEFAULT:
        view = (*it)->CreateDefaultView(login_status);
        break;
      case BUBBLE_TYPE_DETAILED:
        view = (*it)->CreateDetailedView(login_status);
        break;
      case BUBBLE_TYPE_NOTIFICATION:
        view = (*it)->CreateNotificationView(login_status);
        break;
    }
    if (view)
      bubble_view_->AddChildView(new TrayPopupItemContainer(view));
  }

  DCHECK(bubble_widget_ == NULL);
  bubble_widget_ = views::BubbleDelegateView::CreateBubble(bubble_view_);

  // Must occur after call to CreateBubble()
  bubble_view_->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  bubble_widget_->non_client_view()->frame_view()->set_background(NULL);
  SystemTrayBubbleBorder::ArrowType arrow_type;
  if (anchor_type_ == ANCHOR_TYPE_TRAY)
    arrow_type = SystemTrayBubbleBorder::ARROW_TYPE_BOTTOM;
  else
    arrow_type = SystemTrayBubbleBorder::ARROW_TYPE_NONE;
  bubble_view_->SetBubbleBorder(
      new SystemTrayBubbleBorder(bubble_view_, arrow_type));

  bubble_widget_->AddObserver(this);

  // Setup animation.
  ash::SetWindowVisibilityAnimationType(
      bubble_widget_->GetNativeWindow(),
      ash::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ash::SetWindowVisibilityAnimationTransition(
      bubble_widget_->GetNativeWindow(),
      ash::ANIMATE_BOTH);
  ash::SetWindowVisibilityAnimationDuration(
      bubble_widget_->GetNativeWindow(),
      base::TimeDelta::FromMilliseconds(kAnimationDurationForPopupMS));

  bubble_view_->Show();
}

gfx::Rect SystemTrayBubble::GetAnchorRect() const {
  gfx::Rect rect;
  views::Widget* widget = bubble_view()->anchor_widget();
  if (widget->IsVisible()) {
    rect = widget->GetWindowScreenBounds();
    if (anchor_type_ == ANCHOR_TYPE_TRAY) {
      rect.Inset(
          base::i18n::IsRTL() ? kPaddingFromRightEdgeOfScreen : 0,
          0,
          base::i18n::IsRTL() ? 0 : kPaddingFromRightEdgeOfScreen,
          kPaddingFromBottomOfScreen);
    } else if (anchor_type_ == ANCHOR_TYPE_BUBBLE) {
      rect.Inset(
          base::i18n::IsRTL() ? kShadowThickness - 1 : 0,
          0,
          base::i18n::IsRTL() ? 0 : kShadowThickness - 1,
          0);
    }
  }
  return rect;
}

void SystemTrayBubble::DestroyItemViews() {
  for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
       it != items_.end();
       ++it) {
    switch (bubble_type_) {
      case BUBBLE_TYPE_DEFAULT:
        (*it)->DestroyDefaultView();
        break;
      case BUBBLE_TYPE_DETAILED:
        (*it)->DestroyDetailedView();
        break;
      case BUBBLE_TYPE_NOTIFICATION:
        (*it)->DestroyNotificationView();
        break;
    }
  }
}

void SystemTrayBubble::StartAutoCloseTimer(int seconds) {
  autoclose_.Stop();
  autoclose_delay_ = seconds;
  if (autoclose_delay_) {
    autoclose_.Start(FROM_HERE,
                     base::TimeDelta::FromSeconds(autoclose_delay_),
                     this, &SystemTrayBubble::Close);
  }
}

void SystemTrayBubble::StopAutoCloseTimer() {
  autoclose_.Stop();
}

void SystemTrayBubble::RestartAutoCloseTimer() {
  if (autoclose_delay_)
    StartAutoCloseTimer(autoclose_delay_);
}

void SystemTrayBubble::Close() {
  if (bubble_widget_)
    bubble_widget_->Close();
}

base::EventStatus SystemTrayBubble::WillProcessEvent(
    const base::NativeEvent& event) {
  // Check if the user clicked outside of the bubble and close it if they did.
  if (bubble_type_ != BUBBLE_TYPE_NOTIFICATION &&
      ui::EventTypeFromNative(event) == ui::ET_MOUSE_PRESSED) {
    gfx::Point cursor_in_view = ui::EventLocationFromNative(event);
    views::View::ConvertPointFromScreen(bubble_view_, &cursor_in_view);
    if (!bubble_view_->HitTest(cursor_in_view)) {
      bubble_widget_->Close();
    }
  }
  return base::EVENT_CONTINUE;
}

void SystemTrayBubble::DidProcessEvent(const base::NativeEvent& event) {
}

void SystemTrayBubble::OnWidgetClosing(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  MessageLoopForUI::current()->RemoveObserver(this);
  bubble_widget_ = NULL;
  tray_->RemoveBubble(this);
}

void SystemTrayBubble::OnWidgetVisibilityChanged(views::Widget* widget,
                                                 bool visible) {
  if (!visible)
    MessageLoopForUI::current()->RemoveObserver(this);
  else
    MessageLoopForUI::current()->AddObserver(this);
}

// Observe the tray layer animation and update the anchor when it changes.
// TODO(stevenjb): Observe or mirror the actual animation, not just the start
// and end points.
class SystemTrayLayerAnimationObserver : public ui::LayerAnimationObserver {
 public:
  explicit SystemTrayLayerAnimationObserver(SystemTray* host) : host_(host) {}

  virtual void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) {
    host_->UpdateNotificationAnchor();
  }

  virtual void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) {
    host_->UpdateNotificationAnchor();
  }

  virtual void OnLayerAnimationScheduled(ui::LayerAnimationSequence* sequence) {
    host_->UpdateNotificationAnchor();
  }

 private:
  SystemTray* host_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayLayerAnimationObserver);
};

}  // namespace internal

// SystemTray

using internal::SystemTrayBubble;
using internal::SystemTrayLayerAnimationObserver;

SystemTray::SystemTray()
    : items_(),
      accessibility_observer_(NULL),
      audio_observer_(NULL),
      bluetooth_observer_(NULL),
      brightness_observer_(NULL),
      caps_lock_observer_(NULL),
      clock_observer_(NULL),
      drive_observer_(NULL),
      ime_observer_(NULL),
      network_observer_(NULL),
      power_status_observer_(NULL),
      update_observer_(NULL),
      user_observer_(NULL),
      widget_(NULL),
      background_(new internal::SystemTrayBackground),
      should_show_launcher_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(hide_background_animator_(this,
          0, kTrayBackgroundAlpha)),
      ALLOW_THIS_IN_INITIALIZER_LIST(hover_background_animator_(this,
          0, kTrayBackgroundHoverAlpha - kTrayBackgroundAlpha)) {
  tray_container_ = new views::View;
  tray_container_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, 0));
  tray_container_->set_background(background_);
  tray_container_->set_border(
      views::Border::CreateEmptyBorder(1, 1, 1, 1));
  set_border(views::Border::CreateEmptyBorder(0, 0,
        kPaddingFromBottomOfScreen, kPaddingFromRightEdgeOfScreen));
  set_notify_enter_exit_on_child(true);
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(tray_container_);

  // Initially we want to paint the background, but without the hover effect.
  SetPaintsBackground(true, internal::BackgroundAnimator::CHANGE_IMMEDIATE);
  hover_background_animator_.SetPaintsBackground(false,
      internal::BackgroundAnimator::CHANGE_IMMEDIATE);
}

SystemTray::~SystemTray() {
  bubble_.reset();
  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
       it != items_.end();
       ++it) {
    (*it)->DestroyTrayView();
  }
}

void SystemTray::CreateItems() {
  internal::TrayVolume* tray_volume = new internal::TrayVolume();
  internal::TrayBluetooth* tray_bluetooth = new internal::TrayBluetooth();
  internal::TrayBrightness* tray_brightness = new internal::TrayBrightness();
  internal::TrayDate* tray_date = new internal::TrayDate();
  internal::TrayPower* tray_power = new internal::TrayPower();
  internal::TrayNetwork* tray_network = new internal::TrayNetwork;
  internal::TrayUser* tray_user = new internal::TrayUser;
  internal::TrayAccessibility* tray_accessibility =
      new internal::TrayAccessibility;
  internal::TrayCapsLock* tray_caps_lock = new internal::TrayCapsLock;
  internal::TrayDrive* tray_drive = new internal::TrayDrive;
  internal::TrayIME* tray_ime = new internal::TrayIME;
  internal::TrayUpdate* tray_update = new internal::TrayUpdate;

  accessibility_observer_ = tray_accessibility;
  audio_observer_ = tray_volume;
  bluetooth_observer_ = tray_bluetooth;
  brightness_observer_ = tray_brightness;
  caps_lock_observer_ = tray_caps_lock;
  clock_observer_ = tray_date;
  drive_observer_ = tray_drive;
  ime_observer_ = tray_ime;
  network_observer_ = tray_network;
  power_status_observer_ = tray_power;
  update_observer_ = tray_update;
  user_observer_ = tray_user;

  AddTrayItem(tray_user);
  AddTrayItem(new internal::TrayEmpty());
  AddTrayItem(tray_power);
  AddTrayItem(tray_network);
  AddTrayItem(tray_bluetooth);
  AddTrayItem(tray_drive);
  AddTrayItem(tray_ime);
  AddTrayItem(tray_volume);
  AddTrayItem(tray_brightness);
  AddTrayItem(tray_update);
  AddTrayItem(tray_accessibility);
  AddTrayItem(tray_caps_lock);
  AddTrayItem(new internal::TraySettings());
  AddTrayItem(tray_date);
  SetVisible(ash::Shell::GetInstance()->tray_delegate()->
      GetTrayVisibilityOnStartup());
}

void SystemTray::CreateWidget() {
  if (widget_)
    widget_->Close();
  widget_ = new views::Widget;
  internal::StatusAreaView* status_area_view = new internal::StatusAreaView;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  gfx::Size ps = GetPreferredSize();
  params.bounds = gfx::Rect(0, 0, ps.width(), ps.height());
  params.delegate = status_area_view;
  params.parent = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_StatusContainer);
  params.transparent = true;
  widget_->Init(params);
  widget_->set_focus_on_creation(false);
  status_area_view->AddChildView(this);
  widget_->SetContentsView(status_area_view);
  widget_->Show();
  widget_->GetNativeView()->SetName("StatusTrayWidget");

  layer_animation_observer_.reset(new SystemTrayLayerAnimationObserver(this));
  widget_->GetNativeView()->layer()->GetAnimator()->AddObserver(
      layer_animation_observer_.get());
}

void SystemTray::AddTrayItem(SystemTrayItem* item) {
  items_.push_back(item);

  SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
  views::View* tray_item = item->CreateTrayView(delegate->GetUserLoginStatus());
  if (tray_item) {
    tray_container_->AddChildViewAt(tray_item, 0);
    PreferredSizeChanged();
  }
}

void SystemTray::RemoveTrayItem(SystemTrayItem* item) {
  NOTIMPLEMENTED();
}

void SystemTray::ShowDefaultView() {
  ShowItems(items_.get(), false, true);
}

void SystemTray::ShowDetailedView(SystemTrayItem* item,
                                  int close_delay,
                                  bool activate) {
  std::vector<SystemTrayItem*> items;
  items.push_back(item);
  ShowItems(items, true, activate);
  bubble_->StartAutoCloseTimer(close_delay);
}

void SystemTray::ShowNotificationView(SystemTrayItem* item) {
  if (std::find(notification_items_.begin(), notification_items_.end(), item)
      != notification_items_.end())
    return;
  notification_items_.push_back(item);
  UpdateNotificationBubble();
}

void SystemTray::HideNotificationView(SystemTrayItem* item) {
  std::vector<SystemTrayItem*>::iterator found_iter =
      std::find(notification_items_.begin(), notification_items_.end(), item);
  if (found_iter == notification_items_.end())
    return;
  notification_items_.erase(found_iter);
  UpdateNotificationBubble();
}

void SystemTray::SetDetailedViewCloseDelay(int close_delay) {
  if (bubble_.get() &&
      bubble_->bubble_type() == SystemTrayBubble::BUBBLE_TYPE_DETAILED)
    bubble_->StartAutoCloseTimer(close_delay);
}

void SystemTray::UpdateAfterLoginStatusChange(user::LoginStatus login_status) {
  bubble_.reset();

  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->UpdateAfterLoginStatusChange(login_status);
  }

  SetVisible(true);
  PreferredSizeChanged();
}

bool SystemTray::CloseBubbleForTest() const {
  if (!bubble_.get())
    return false;
  bubble_->Close();
  return true;
}

// Private methods.

void SystemTray::RemoveBubble(SystemTrayBubble* bubble) {
  if (bubble == bubble_.get()) {
    bubble_.reset();
    UpdateNotificationBubble();  // State changed, re-create notifications.
    if (should_show_launcher_) {
      // No need to show the launcher if the mouse isn't over the status area
      // anymore.
      aura::RootWindow* root = GetWidget()->GetNativeView()->GetRootWindow();
      should_show_launcher_ = GetWidget()->GetWindowScreenBounds().Contains(
          root->last_mouse_location());
      if (!should_show_launcher_)
        Shell::GetInstance()->shelf()->UpdateAutoHideState();
    }
  } else if (bubble == notification_bubble_) {
    notification_bubble_.reset();
  } else {
    NOTREACHED();
  }
}

void SystemTray::SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type) {
  hide_background_animator_.SetPaintsBackground(value, change_type);
}

void SystemTray::ShowItems(const std::vector<SystemTrayItem*>& items,
                           bool detailed,
                           bool can_activate) {
  // Destroy any existing bubble and create a new one.
  SystemTrayBubble::BubbleType bubble_type = detailed ?
      SystemTrayBubble::BUBBLE_TYPE_DETAILED :
      SystemTrayBubble::BUBBLE_TYPE_DEFAULT;
  bubble_.reset(new SystemTrayBubble(this, items, bubble_type));
  ash::SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->tray_delegate();
  views::View* anchor = tray_container_;
  bubble_->InitView(anchor, SystemTrayBubble::ANCHOR_TYPE_TRAY,
                    can_activate, delegate->GetUserLoginStatus());
  // If we have focus the shelf should be visible and we need to continue
  // showing the shelf when the popup is shown.
  if (GetWidget()->IsActive())
    should_show_launcher_ = true;
  UpdateNotificationBubble();  // State changed, re-create notifications.
}

void SystemTray::UpdateNotificationBubble() {
  // Only show the notification buble if we have notifications and we are not
  // showing the default bubble.
  if (notification_items_.empty() ||
      (bubble_.get() &&
       bubble_->bubble_type() == SystemTrayBubble::BUBBLE_TYPE_DEFAULT)) {
    notification_bubble_.reset();
    return;
  }
  notification_bubble_.reset(
      new SystemTrayBubble(this, notification_items_,
                           SystemTrayBubble::BUBBLE_TYPE_NOTIFICATION));
  views::View* anchor;
  SystemTrayBubble::AnchorType anchor_type;
  if (bubble_.get()) {
    anchor = bubble_->bubble_view();
    anchor_type = SystemTrayBubble::ANCHOR_TYPE_BUBBLE;
  } else {
    anchor = tray_container_;
    anchor_type = SystemTrayBubble::ANCHOR_TYPE_TRAY;
  }
  notification_bubble_->InitView(
      anchor, anchor_type,
      false /* can_activate */,
      ash::Shell::GetInstance()->tray_delegate()->GetUserLoginStatus());
}

void SystemTray::UpdateNotificationAnchor() {
  if (!notification_bubble_.get())
    return;
  notification_bubble_->bubble_view()->UpdateAnchor();
  // Ensure that the notification buble is above the launcher/status area.
  notification_bubble_->bubble_view()->GetWidget()->StackAtTop();
}

bool SystemTray::PerformAction(const views::Event& event) {
  // If we're already showing the default view, hide it; otherwise, show it
  // (and hide any popup that's currently shown).
  if (bubble_.get() &&
      bubble_->bubble_type() == SystemTrayBubble::BUBBLE_TYPE_DEFAULT) {
    bubble_->Close();
  } else {
    ShowDefaultView();
  }
  return true;
}

void SystemTray::OnMouseEntered(const views::MouseEvent& event) {
  should_show_launcher_ = true;
  hover_background_animator_.SetPaintsBackground(true,
      internal::BackgroundAnimator::CHANGE_ANIMATE);
}

void SystemTray::OnMouseExited(const views::MouseEvent& event) {
  // When the popup closes we'll update |should_show_launcher_|.
  if (!bubble_.get())
    should_show_launcher_ = false;
  hover_background_animator_.SetPaintsBackground(false,
      internal::BackgroundAnimator::CHANGE_ANIMATE);
}

void SystemTray::AboutToRequestFocusFromTabTraversal(bool reverse) {
  views::View* v = GetNextFocusableView();
  if (v)
    v->AboutToRequestFocusFromTabTraversal(reverse);
}

void SystemTray::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBLE_NAME);
}

void SystemTray::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // The tray itself expands to the right and bottom edge of the screen to make
  // sure clicking on the edges brings up the popup. However, the focus border
  // should be only around the container.
  if (GetWidget() && GetWidget()->IsActive())
    canvas->DrawFocusRect(tray_container_->bounds());
}

void SystemTray::UpdateBackground(int alpha) {
  background_->set_alpha(hide_background_animator_.alpha() +
                         hover_background_animator_.alpha());
  SchedulePaint();
}

}  // namespace ash
