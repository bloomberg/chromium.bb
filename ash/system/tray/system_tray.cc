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
#include "ash/system/locale/tray_locale.h"
#include "ash/system/network/tray_network.h"
#include "ash/system/network/tray_sms.h"
#include "ash/system/power/power_status_observer.h"
#include "ash/system/power/power_supply_status.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/settings/tray_settings.h"
#include "ash/system/tray/system_tray_bubble.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
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

namespace ash {

namespace internal {

// Container for all the items in the tray. The container auto-resizes the
// widget when necessary.
class SystemTrayContainer : public views::View {
 public:
  SystemTrayContainer() {}
  virtual ~SystemTrayContainer() {}

  void SetLayoutManager(views::LayoutManager* layout_manager) {
    views::View::SetLayoutManager(layout_manager);
    PreferredSizeChanged();
  }

 private:
  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE {
    PreferredSizeChanged();
  }

  virtual void ChildVisibilityChanged(View* child) OVERRIDE {
    PreferredSizeChanged();
  }

  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE {
    if (parent == this)
      PreferredSizeChanged();
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
      locale_observer_(NULL),
      network_observer_(NULL),
      update_observer_(NULL),
      user_observer_(NULL),
      should_show_launcher_(false),
      default_bubble_height_(0),
      hide_notifications_(false) {
  tray_container_ = new internal::SystemTrayContainer;
  tray_container_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, 0));
  tray_container_->set_border(
      views::Border::CreateEmptyBorder(1, 1, 1, 1));
  SetContents(tray_container_);
  SetBorder();
}

SystemTray::~SystemTray() {
  bubble_.reset();
  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
       it != items_.end();
       ++it) {
    (*it)->DestroyTrayView();
  }
}

void SystemTray::Initialize() {
  layer_animation_observer_.reset(new SystemTrayLayerAnimationObserver(this));
  GetWidget()->GetNativeView()->layer()->GetAnimator()->AddObserver(
      layer_animation_observer_.get());
}

void SystemTray::CreateItems() {
  internal::TrayVolume* tray_volume = new internal::TrayVolume();
  internal::TrayBluetooth* tray_bluetooth = new internal::TrayBluetooth();
  internal::TrayBrightness* tray_brightness = new internal::TrayBrightness();
  internal::TrayDate* tray_date = new internal::TrayDate();
  internal::TrayPower* tray_power = new internal::TrayPower();
  internal::TrayNetwork* tray_network = new internal::TrayNetwork;
  internal::TraySms* tray_sms = new internal::TraySms();
  internal::TrayUser* tray_user = new internal::TrayUser;
  internal::TrayAccessibility* tray_accessibility =
      new internal::TrayAccessibility;
  internal::TrayCapsLock* tray_caps_lock = new internal::TrayCapsLock;
  internal::TrayDrive* tray_drive = new internal::TrayDrive;
  internal::TrayIME* tray_ime = new internal::TrayIME;
  internal::TrayLocale* tray_locale = new internal::TrayLocale;
  internal::TrayUpdate* tray_update = new internal::TrayUpdate;
  internal::TraySettings* tray_settings = new internal::TraySettings();

  accessibility_observer_ = tray_accessibility;
  audio_observer_ = tray_volume;
  bluetooth_observer_ = tray_bluetooth;
  brightness_observer_ = tray_brightness;
  caps_lock_observer_ = tray_caps_lock;
  clock_observer_ = tray_date;
  drive_observer_ = tray_drive;
  ime_observer_ = tray_ime;
  locale_observer_ = tray_locale;
  network_observer_ = tray_network;
  power_status_observers_.AddObserver(tray_power);
  power_status_observers_.AddObserver(tray_settings);
  update_observer_ = tray_update;
  user_observer_ = tray_user;

  AddTrayItem(tray_user);
  AddTrayItem(tray_power);
  AddTrayItem(tray_network);
  AddTrayItem(tray_bluetooth);
  AddTrayItem(tray_sms);
  AddTrayItem(tray_drive);
  AddTrayItem(tray_ime);
  AddTrayItem(tray_locale);
  AddTrayItem(tray_volume);
  AddTrayItem(tray_brightness);
  AddTrayItem(tray_update);
  AddTrayItem(tray_accessibility);
  AddTrayItem(tray_caps_lock);
  AddTrayItem(tray_settings);
  AddTrayItem(tray_date);
  SetVisible(ash::Shell::GetInstance()->tray_delegate()->
      GetTrayVisibilityOnStartup());
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

void SystemTray::SetDetailedViewCloseDelay(int close_delay) {
  if (bubble_.get() &&
      bubble_->bubble_type() == SystemTrayBubble::BUBBLE_TYPE_DETAILED)
    bubble_->StartAutoCloseTimer(close_delay);
}

void SystemTray::HideDetailedView(SystemTrayItem* item) {
  if (item != detailed_item_)
    return;
  DestroyBubble();
  UpdateNotificationBubble();
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
  // Only update the notification bubble if visible (i.e. don't create one).
  if (notification_bubble_.get())
    UpdateNotificationBubble();
}

void SystemTray::UpdateAfterLoginStatusChange(user::LoginStatus login_status) {
  DestroyBubble();

  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->UpdateAfterLoginStatusChange(login_status);
  }

  SetVisible(true);
  PreferredSizeChanged();
}

void SystemTray::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->UpdateAfterShelfAlignmentChange(alignment);
  }
}

void SystemTray::SetHideNotifications(bool hide_notifications) {
  if (notification_bubble_.get())
    notification_bubble_->SetVisible(!hide_notifications);
  hide_notifications_ = hide_notifications;
}

bool SystemTray::IsBubbleVisible() const {
  return bubble_.get() && bubble_->IsVisible();
}

bool SystemTray::CloseBubbleForTest() const {
  if (!bubble_.get())
    return false;
  bubble_->Close();
  return true;
}

// Private methods.

void SystemTray::DestroyBubble() {
  bubble_.reset();
  detailed_item_ = NULL;
}

void SystemTray::RemoveBubble(SystemTrayBubble* bubble) {
  if (bubble == bubble_.get()) {
    DestroyBubble();
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

  // Destroy the notification bubble here so that it doesn't get rebuilt
  // while we add items to the main bubble_ (e.g. in HideNotificationView).
  notification_bubble_.reset();

  if (bubble_.get() && creation_type == BUBBLE_USE_EXISTING) {
    bubble_->UpdateView(items, bubble_type);
  } else {
    bubble_.reset(new SystemTrayBubble(this, items, bubble_type));
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    views::View* anchor = tray_container_;
    SystemTrayBubble::InitParams init_params(
        SystemTrayBubble::ANCHOR_TYPE_TRAY,
        shelf_alignment());
    init_params.anchor = anchor;
    init_params.can_activate = can_activate;
    init_params.login_status = delegate->GetUserLoginStatus();
    if (arrow_offset >= 0)
      init_params.arrow_offset = arrow_offset;
    if (detailed)
      init_params.max_height = default_bubble_height_;
    bubble_->InitView(init_params);
  }
  // Save height of default view for creating detailed views directly.
  if (!detailed)
    default_bubble_height_ = bubble_->bubble_view()->height();

  if (detailed && items.size() > 0)
    detailed_item_ = items[0];
  else
    detailed_item_ = NULL;

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
  if (bubble_.get() &&
      bubble_->bubble_type() == SystemTrayBubble::BUBBLE_TYPE_DETAILED) {
    // Skip notifications for any currently displayed detailed item.
    std::vector<SystemTrayItem*> items;
    for (std::vector<SystemTrayItem*>::iterator iter =
             notification_items_.begin();
         iter != notification_items_.end(); ++ iter) {
      if (*iter != detailed_item_)
        items.push_back(*iter);
    }
    notification_bubble_.reset(new SystemTrayBubble(
        this, items, SystemTrayBubble::BUBBLE_TYPE_NOTIFICATION));
  } else {
    // Show all notifications.
    notification_bubble_.reset(new SystemTrayBubble(
        this, notification_items_, SystemTrayBubble::BUBBLE_TYPE_NOTIFICATION));
  }
  views::View* anchor;
  SystemTrayBubble::AnchorType anchor_type;
  if (bubble_.get()) {
    anchor = bubble_->bubble_view();
    anchor_type = SystemTrayBubble::ANCHOR_TYPE_BUBBLE;
  } else {
    anchor = tray_container_;
    anchor_type = SystemTrayBubble::ANCHOR_TYPE_TRAY;
  }
  SystemTrayBubble::InitParams init_params(anchor_type, shelf_alignment());
  init_params.anchor = anchor;
  init_params.login_status =
      ash::Shell::GetInstance()->tray_delegate()->GetUserLoginStatus();
  int arrow_offset = GetTrayXOffset(notification_items_[0]);
  if (arrow_offset >= 0)
    init_params.arrow_offset = arrow_offset;
  notification_bubble_->InitView(init_params);
  if (hide_notifications_)
    notification_bubble_->SetVisible(false);
}

void SystemTray::UpdateNotificationAnchor() {
  if (!notification_bubble_.get())
    return;
  notification_bubble_->bubble_view()->UpdateAnchor();
  // Ensure that the notification buble is above the launcher/status area.
  notification_bubble_->bubble_view()->GetWidget()->StackAtTop();
}

void SystemTray::SetBorder() {
  // Change the border padding for different shelf alignment.
  if (shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
    set_border(views::Border::CreateEmptyBorder(0, 0,
        kPaddingFromBottomOfScreenBottomAlignment,
        kPaddingFromRightEdgeOfScreenBottomAlignment));
  } else if (shelf_alignment() == SHELF_ALIGNMENT_LEFT) {
    set_border(views::Border::CreateEmptyBorder(0,
        kPaddingFromEdgeOfScreenVerticalAlignment,
        kPaddingFromBottomOfScreenVerticalAlignment,
        kPaddingFromEdgeOfLauncherVerticalAlignment));
  } else {
    set_border(views::Border::CreateEmptyBorder(0,
        kPaddingFromEdgeOfLauncherVerticalAlignment,
        kPaddingFromBottomOfScreenVerticalAlignment,
        kPaddingFromEdgeOfScreenVerticalAlignment));
  }
}

void SystemTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  internal::TrayBackgroundView::SetShelfAlignment(alignment);
  UpdateAfterShelfAlignmentChange(alignment);
  SetBorder();
  tray_container_->SetLayoutManager(new views::BoxLayout(
      alignment == SHELF_ALIGNMENT_BOTTOM ?
          views::BoxLayout::kHorizontal : views::BoxLayout::kVertical,
      0, 0, 0));
}

bool SystemTray::PerformAction(const views::Event& event) {
  // If we're already showing the default view, hide it; otherwise, show it
  // (and hide any popup that's currently shown).
  if (bubble_.get() &&
      bubble_->bubble_type() == SystemTrayBubble::BUBBLE_TYPE_DEFAULT) {
    bubble_->Close();
  } else {
    int arrow_offset = -1;
    if (event.IsMouseEvent() || event.type() == ui::ET_GESTURE_TAP) {
      const views::LocatedEvent& located_event =
          static_cast<const views::LocatedEvent&>(event);
      if (shelf_alignment() == SHELF_ALIGNMENT_BOTTOM)
        arrow_offset = base::i18n::IsRTL() ?
            located_event.x() : tray_container_->width() - located_event.x();
    }
    ShowDefaultViewWithOffset(BUBBLE_CREATE_NEW, arrow_offset);
  }
  return true;
}

void SystemTray::OnMouseEntered(const views::MouseEvent& event) {
  TrayBackgroundView::OnMouseEntered(event);
  should_show_launcher_ = true;
}

void SystemTray::OnMouseExited(const views::MouseEvent& event) {
  TrayBackgroundView::OnMouseExited(event);
  // When the popup closes we'll update |should_show_launcher_|.
  if (!bubble_.get())
    should_show_launcher_ = false;
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

}  // namespace ash
