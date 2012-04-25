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
#include "ash/system/ime/tray_ime.h"
#include "ash/system/network/tray_network.h"
#include "ash/system/power/power_status_observer.h"
#include "ash/system/power/power_supply_status.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/settings/tray_settings.h"
#include "ash/system/tray/tray_empty.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/system_tray_widget_delegate.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/tray_caps_lock.h"
#include "ash/system/tray_update.h"
#include "ash/system/user/tray_user.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/shadow_types.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/root_window.h"
#include "ui/base/events.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/box_layout.h"
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

const int kShadowOffset = 3;
const int kShadowHeight = 3;

const int kLeftPadding = 4;
const int kBottomLineHeight = 1;

const SkColor kShadowColor = SkColorSetARGB(25, 0, 0, 0);

const SkColor kTrayBackgroundAlpha = 100;
const SkColor kTrayBackgroundHoverAlpha = 150;

// A view with some special behaviour for tray items in the popup:
// - changes background color on hover.
class TrayPopupItemContainer : public views::View {
 public:
  explicit TrayPopupItemContainer(views::View* view) : hover_(false) {
    set_notify_enter_exit_on_child(true);
    set_border(view->border() ? views::Border::CreateEmptyBorder(0, 0, 0, 0) :
                                NULL);
    SetLayoutManager(new views::FillLayout);
    AddChildView(view);
  }

  virtual ~TrayPopupItemContainer() {}

 private:
  // Overridden from views::View.
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
    } else {
      canvas->FillRect(gfx::Rect(view->x() + kShadowOffset, view->y(),
                                 view->width() - kShadowOffset, kShadowHeight),
                       kShadowColor);
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

class SystemTrayBubbleBorder : public views::Border {
 public:
  explicit SystemTrayBubbleBorder(views::View* owner)
      : owner_(owner) {
  }

  virtual ~SystemTrayBubbleBorder() {}

 private:
  // Overridden from views::Border.
  virtual void Paint(const views::View& view,
                     gfx::Canvas* canvas) const OVERRIDE {
    // Draw a line first.
    int y = owner_->height() + 1;
    canvas->FillRect(gfx::Rect(kLeftPadding, y, owner_->width(),
                               kBottomLineHeight), kBorderDarkColor);

    // Now, draw a shadow.
    canvas->FillRect(gfx::Rect(kLeftPadding + kShadowOffset, y,
                               owner_->width() - kShadowOffset, kShadowHeight),
                     kShadowColor);

    if (Shell::GetInstance()->shelf()->IsVisible()) {
      // Draw the arrow.
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

      // Now the draw the outline.
      paint.setStyle(SkPaint::kStroke_Style);
      paint.setColor(kBorderDarkColor);
      canvas->DrawPath(path, paint);
    }
  }

  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE {
    insets->Set(0, 0, kArrowHeight, 0);
  }

  views::View* owner_;

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

class SystemTrayBubble : public views::BubbleDelegateView {
 public:
  SystemTrayBubble(ash::SystemTray* tray,
                   views::View* anchor,
                   std::vector<ash::SystemTrayItem*>& items,
                   bool detailed)
      : views::BubbleDelegateView(anchor, views::BubbleBorder::BOTTOM_RIGHT),
        tray_(tray),
        items_(items),
        detailed_(detailed),
        can_activate_(true),
        autoclose_delay_(0) {
    set_margin(0);
    set_parent_window(ash::Shell::GetInstance()->GetContainer(
        ash::internal::kShellWindowId_SettingBubbleContainer));
    set_notify_enter_exit_on_child(true);
  }

  virtual ~SystemTrayBubble() {
    for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
        it != items_.end();
        ++it) {
      if (detailed_)
        (*it)->DestroyDetailedView();
      else
        (*it)->DestroyDefaultView();
    }
  }

  bool detailed() const { return detailed_; }
  void set_can_activate(bool activate) { can_activate_ = activate; }

  void StartAutoCloseTimer(int seconds) {
    autoclose_.Stop();
    autoclose_delay_ = seconds;
    if (autoclose_delay_) {
      autoclose_.Start(FROM_HERE,
          base::TimeDelta::FromSeconds(autoclose_delay_),
          this, &SystemTrayBubble::AutoClose);
    }
  }

 private:
  void AutoClose() {
    GetWidget()->Close();
  }

  // Overridden from views::BubbleDelegateView.
  virtual void Init() OVERRIDE {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
          1, 1, 1));
    set_background(new SystemTrayBubbleBackground(this));

    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    ash::user::LoginStatus login_status = delegate->GetUserLoginStatus();
    for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
        it != items_.end();
        ++it) {
      views::View* view = detailed_ ? (*it)->CreateDetailedView(login_status) :
                                      (*it)->CreateDefaultView(login_status);
      if (view)
        AddChildView(new TrayPopupItemContainer(view));
    }
  }

  virtual gfx::Rect GetAnchorRect() OVERRIDE {
    views::Widget* widget = tray_->GetWidget();
    if (widget->IsVisible()) {
      gfx::Rect rect = widget->GetWindowScreenBounds();
      rect.Inset(
          base::i18n::IsRTL() ? kPaddingFromRightEdgeOfScreen : 0, 0,
          base::i18n::IsRTL() ? 0 : kPaddingFromRightEdgeOfScreen,
          kPaddingFromBottomOfScreen);
      return rect;
    }
    gfx::Rect rect = gfx::Screen::GetPrimaryMonitor().bounds();
    return gfx::Rect(base::i18n::IsRTL() ? kPaddingFromRightEdgeOfScreen :
                        rect.width() - kPaddingFromRightEdgeOfScreen,
                     rect.height() - kPaddingFromBottomOfScreen,
                     0, 0);
  }

  // Overridden from views::View.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE {
    if (can_activate_) {
      state->role = ui::AccessibilityTypes::ROLE_WINDOW;
      state->name = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBLE_NAME);
    }
  }

  virtual bool CanActivate() const OVERRIDE {
    return can_activate_;
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size = views::BubbleDelegateView::GetPreferredSize();
    return gfx::Size(kTrayPopupWidth, size.height());
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    autoclose_.Stop();
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    if (autoclose_delay_) {
      autoclose_.Stop();
      autoclose_.Start(FROM_HERE,
          base::TimeDelta::FromSeconds(autoclose_delay_),
          this, &SystemTrayBubble::AutoClose);
    }
  }

  ash::SystemTray* tray_;
  std::vector<ash::SystemTrayItem*> items_;
  bool detailed_;
  bool can_activate_;

  int autoclose_delay_;
  base::OneShotTimer<SystemTrayBubble> autoclose_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubble);
};

}  // namespace internal

NetworkIconInfo::NetworkIconInfo()
    : highlight(false),
      tray_icon_visible(true) {
}

NetworkIconInfo::~NetworkIconInfo() {
}

BluetoothDeviceInfo::BluetoothDeviceInfo()
    : connected(false) {
}

BluetoothDeviceInfo::~BluetoothDeviceInfo() {
}

IMEInfo::IMEInfo()
    : selected(false) {
}

IMEInfo::~IMEInfo() {
}

IMEPropertyInfo::IMEPropertyInfo()
    : selected(false) {
}

IMEPropertyInfo::~IMEPropertyInfo() {
}

SystemTray::SystemTray()
    : items_(),
      accessibility_observer_(NULL),
      audio_observer_(NULL),
      bluetooth_observer_(NULL),
      brightness_observer_(NULL),
      caps_lock_observer_(NULL),
      clock_observer_(NULL),
      ime_observer_(NULL),
      network_observer_(NULL),
      power_status_observer_(NULL),
      update_observer_(NULL),
      user_observer_(NULL),
      widget_(NULL),
      bubble_(NULL),
      popup_(NULL),
      background_(new internal::SystemTrayBackground),
      should_show_launcher_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(hide_background_animator_(this,
          0, kTrayBackgroundAlpha)),
      ALLOW_THIS_IN_INITIALIZER_LIST(hover_background_animator_(this,
          0, kTrayBackgroundHoverAlpha - kTrayBackgroundAlpha)) {
  container_ = new views::View;
  container_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, 0));
  container_->set_background(background_);
  container_->set_border(
      views::Border::CreateEmptyBorder(1, 1, 1, 1));
  set_border(views::Border::CreateEmptyBorder(0, 0,
        kPaddingFromBottomOfScreen, kPaddingFromRightEdgeOfScreen));
  set_notify_enter_exit_on_child(true);
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(container_);

  // Initially we want to paint the background, but without the hover effect.
  SetPaintsBackground(true, internal::BackgroundAnimator::CHANGE_IMMEDIATE);
  hover_background_animator_.SetPaintsBackground(false,
      internal::BackgroundAnimator::CHANGE_IMMEDIATE);
}

SystemTray::~SystemTray() {
  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->DestroyTrayView();
  }
  if (popup_)
    popup_->CloseNow();
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
  internal::TrayIME* tray_ime = new internal::TrayIME;
  internal::TrayUpdate* tray_update = new internal::TrayUpdate;

  accessibility_observer_ = tray_accessibility;
  audio_observer_ = tray_volume;
  bluetooth_observer_ = tray_bluetooth;
  brightness_observer_ = tray_brightness;
  caps_lock_observer_ = tray_caps_lock;
  clock_observer_ = tray_date;
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
  AddTrayItem(tray_ime);
  AddTrayItem(tray_volume);
  AddTrayItem(tray_brightness);
  AddTrayItem(tray_update);
  AddTrayItem(new internal::TraySettings());
  AddTrayItem(tray_accessibility);
  AddTrayItem(tray_caps_lock);
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
}

void SystemTray::AddTrayItem(SystemTrayItem* item) {
  items_.push_back(item);

  SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
  views::View* tray_item = item->CreateTrayView(delegate->GetUserLoginStatus());
  if (tray_item) {
    container_->AddChildViewAt(tray_item, 0);
    PreferredSizeChanged();
  }
}

void SystemTray::RemoveTrayItem(SystemTrayItem* item) {
  NOTIMPLEMENTED();
}

void SystemTray::ShowDefaultView() {
  if (popup_) {
    MessageLoopForUI::current()->RemoveObserver(this);
    popup_->RemoveObserver(this);
    popup_->Close();
  }
  popup_ = NULL;
  bubble_ = NULL;

  ShowItems(items_.get(), false, true);
}

void SystemTray::ShowDetailedView(SystemTrayItem* item,
                                  int close_delay,
                                  bool activate) {
  if (popup_) {
    MessageLoopForUI::current()->RemoveObserver(this);
    popup_->RemoveObserver(this);
    popup_->Close();
  }
  popup_ = NULL;
  bubble_ = NULL;

  std::vector<SystemTrayItem*> items;
  items.push_back(item);
  ShowItems(items, true, activate);
  bubble_->StartAutoCloseTimer(close_delay);
}

void SystemTray::SetDetailedViewCloseDelay(int close_delay) {
  if (bubble_ && bubble_->detailed())
    bubble_->StartAutoCloseTimer(close_delay);
}

void SystemTray::UpdateAfterLoginStatusChange(user::LoginStatus login_status) {
  if (popup_)
    popup_->CloseNow();

  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->UpdateAfterLoginStatusChange(login_status);
  }

  SetVisible(true);
  PreferredSizeChanged();
}

void SystemTray::SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type) {
  hide_background_animator_.SetPaintsBackground(value, change_type);
}

void SystemTray::ShowItems(std::vector<SystemTrayItem*>& items,
                           bool detailed,
                           bool activate) {
  CHECK(!popup_);
  CHECK(!bubble_);
  bubble_ = new internal::SystemTrayBubble(this, container_, items, detailed);
  bubble_->set_can_activate(activate);
  popup_ = views::BubbleDelegateView::CreateBubble(bubble_);
  // If we have focus the shelf should be visible and we need to continue
  // showing the shelf when the popup is shown.
  if (GetWidget()->IsActive())
    should_show_launcher_ = true;
  bubble_->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  popup_->non_client_view()->frame_view()->set_background(NULL);
  popup_->non_client_view()->frame_view()->set_border(
      new SystemTrayBubbleBorder(bubble_));
  MessageLoopForUI::current()->AddObserver(this);
  popup_->AddObserver(this);

  // Setup animation.
  ash::SetWindowVisibilityAnimationType(popup_->GetNativeWindow(),
      ash::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ash::SetWindowVisibilityAnimationTransition(popup_->GetNativeWindow(),
      ash::ANIMATE_BOTH);
  ash::SetWindowVisibilityAnimationDuration(popup_->GetNativeWindow(),
      base::TimeDelta::FromMilliseconds(kAnimationDurationForPopupMS));

  bubble_->Show();
}

bool SystemTray::PerformAction(const views::Event& event) {
  // If we're already showing the default view, hide it; otherwise, show it
  // (and hide any popup that's currently shown).
  if (popup_ && bubble_ && !bubble_->detailed())
    popup_->Hide();
  else
    ShowDefaultView();
  return true;
}

void SystemTray::OnMouseEntered(const views::MouseEvent& event) {
  should_show_launcher_ = true;
  hover_background_animator_.SetPaintsBackground(true,
      internal::BackgroundAnimator::CHANGE_ANIMATE);
}

void SystemTray::OnMouseExited(const views::MouseEvent& event) {
  // When the popup closes we'll update |should_show_launcher_|.
  if (!popup_)
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
    canvas->DrawFocusRect(container_->bounds());
}

void SystemTray::OnWidgetClosing(views::Widget* widget) {
  CHECK_EQ(popup_, widget);
  MessageLoopForUI::current()->RemoveObserver(this);
  popup_ = NULL;
  bubble_ = NULL;
  if (should_show_launcher_) {
    // No need to show the launcher if the mouse isn't over the status area
    // anymore.
    aura::RootWindow* root = GetWidget()->GetNativeView()->GetRootWindow();
    should_show_launcher_ = GetWidget()->GetWindowScreenBounds().Contains(
        root->last_mouse_location());
    if (!should_show_launcher_)
      Shell::GetInstance()->shelf()->UpdateAutoHideState();
  }
}

void SystemTray::OnWidgetVisibilityChanged(views::Widget* widget,
                                           bool visible) {
  if (!visible)
    MessageLoopForUI::current()->RemoveObserver(this);
}

void SystemTray::UpdateBackground(int alpha) {
  background_->set_alpha(hide_background_animator_.alpha() +
                         hover_background_animator_.alpha());
  SchedulePaint();
}

base::EventStatus SystemTray::WillProcessEvent(const base::NativeEvent& event) {
  // Check if the user clicked outside of the system tray bubble and hide it
  // if they did.
  if (bubble_ && ui::EventTypeFromNative(event) == ui::ET_MOUSE_PRESSED) {
    gfx::Point cursor_in_view = ui::EventLocationFromNative(event);
    View::ConvertPointFromScreen(bubble_, &cursor_in_view);
    if (!bubble_->HitTest(cursor_in_view)) {
      popup_->Hide();
    }
  }
  return base::EVENT_CONTINUE;
}

void SystemTray::DidProcessEvent(const base::NativeEvent& event) {
}

}  // namespace ash
