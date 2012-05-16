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
#include "ash/system/tray/system_tray_bubble.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/system_tray_widget_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/tray_caps_lock.h"
#include "ash/system/tray_update.h"
#include "ash/system/user/login_status.h"
#include "ash/system/user/tray_user.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/aura/root_window.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/events.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const SkColor kTrayBackgroundAlpha = 100;
const SkColor kTrayBackgroundHoverAlpha = 150;

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

// Container for all the items in the tray. The container auto-resizes the
// widget when necessary.
class SystemTrayContainer : public views::View {
 public:
  SystemTrayContainer() {}
  virtual ~SystemTrayContainer() {}

 private:
  void UpdateWidgetSize() {
    if (GetWidget())
      GetWidget()->SetSize(GetWidget()->GetContentsView()->GetPreferredSize());
  }

  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(views::View* child) {
    views::View::ChildPreferredSizeChanged(child);
    UpdateWidgetSize();
  }

  virtual void ChildVisibilityChanged(View* child) OVERRIDE {
    views::View::ChildVisibilityChanged(child);
    UpdateWidgetSize();
  }

  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE {
    if (parent == this)
      UpdateWidgetSize();
  }

  DISALLOW_COPY_AND_ASSIGN(SystemTrayContainer);
};

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
  tray_container_ = new internal::SystemTrayContainer;
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
    tray_item_map_[item] = tray_item;
  }
}

void SystemTray::RemoveTrayItem(SystemTrayItem* item) {
  NOTIMPLEMENTED();
}

void SystemTray::ShowDefaultView(BubbleCreationType creation_type) {
  ShowDefaultViewWithOffset(creation_type, -1);
}

void SystemTray::ShowDetailedView(SystemTrayItem* item,
                                  int close_delay,
                                  bool activate,
                                  BubbleCreationType creation_type) {
  std::vector<SystemTrayItem*> items;
  items.push_back(item);
  ShowItems(items, true, activate, creation_type, GetTrayXOffset(item));
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

int SystemTray::GetTrayXOffset(SystemTrayItem* item) const {
  std::map<SystemTrayItem*, views::View*>::const_iterator it =
      tray_item_map_.find(item);
  if (it == tray_item_map_.end())
    return -1;

  const views::View* item_view = it->second;
  gfx::Rect item_bounds = item_view->bounds();
  if (!item_bounds.IsEmpty()) {
    int x_offset = item_bounds.x() + item_bounds.width() / 2;
    return base::i18n::IsRTL() ? x_offset : tray_container_->width() - x_offset;
  }

  // The bounds of item could be still empty.  It could happen in the case that
  // the view appears for the first time in the current session, because the
  // bounds is not calculated yet. In that case, we want to guess the offset
  // from the position of its parent.
  int x_offset = 0;
  for (int i = 0; i < tray_container_->child_count(); ++i) {
    const views::View* child = tray_container_->child_at(i);
    if (child == item_view)
      return base::i18n::IsRTL() ?
          x_offset : tray_container_->width() - x_offset;

    if (!child->visible())
      continue;
    x_offset = child->bounds().right();
  }

  return -1;
}

void SystemTray::ShowDefaultViewWithOffset(BubbleCreationType creation_type,
                                           int arrow_offset) {
  ShowItems(items_.get(), false, true, creation_type, arrow_offset);
}

void SystemTray::ShowItems(const std::vector<SystemTrayItem*>& items,
                           bool detailed,
                           bool can_activate,
                           BubbleCreationType creation_type,
                           int arrow_offset) {
  // Destroy any existing bubble and create a new one.
  SystemTrayBubble::BubbleType bubble_type = detailed ?
      SystemTrayBubble::BUBBLE_TYPE_DETAILED :
      SystemTrayBubble::BUBBLE_TYPE_DEFAULT;
  if (bubble_.get() && creation_type == BUBBLE_USE_EXISTING) {
    bubble_->UpdateView(items, bubble_type);
  } else {
    bubble_.reset(new SystemTrayBubble(this, items, bubble_type));
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    views::View* anchor = tray_container_;
    SystemTrayBubble::InitParams init_params(
        SystemTrayBubble::ANCHOR_TYPE_TRAY);
    init_params.anchor = anchor;
    init_params.can_activate = can_activate;
    init_params.login_status = delegate->GetUserLoginStatus();
    if (arrow_offset >= 0)
      init_params.arrow_offset = arrow_offset;
    bubble_->InitView(init_params);
  }
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
  SystemTrayBubble::InitParams init_params(anchor_type);
  init_params.anchor = anchor;
  init_params.login_status =
      ash::Shell::GetInstance()->tray_delegate()->GetUserLoginStatus();
  int arrow_offset = GetTrayXOffset(notification_items_[0]);
  if (arrow_offset >= 0)
    init_params.arrow_offset = arrow_offset;
  notification_bubble_->InitView(init_params);
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
    int arrow_offset = -1;
    if (event.IsMouseEvent() || event.IsTouchEvent()) {
      const views::LocatedEvent& located_event =
          static_cast<const views::LocatedEvent&>(event);
      arrow_offset = base::i18n::IsRTL() ?
          located_event.x() : tray_container_->width() - located_event.x();
    }
    ShowDefaultViewWithOffset(BUBBLE_CREATE_NEW, arrow_offset);
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
