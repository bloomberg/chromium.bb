// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_SCREEN_SECURITY_SCREEN_TRAY_ITEM_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_SCREEN_SECURITY_SCREEN_TRAY_ITEM_H_

#include <string>

#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/system/tray/tray_notification_view.h"
#include "base/macros.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"

namespace views {
class View;
}

namespace ash {
class ScreenTrayItem;

namespace tray {

class ScreenTrayView : public TrayItemView {
 public:
  explicit ScreenTrayView(ScreenTrayItem* screen_tray_item);
  ~ScreenTrayView() override;

  void Update();

 private:
  ScreenTrayItem* screen_tray_item_;

  DISALLOW_COPY_AND_ASSIGN(ScreenTrayView);
};

class ScreenStatusView : public views::View, public views::ButtonListener {
 public:
  ScreenStatusView(ScreenTrayItem* screen_tray_item,
                   const base::string16& label_text,
                   const base::string16& stop_button_text);
  ~ScreenStatusView() override;

  // Overridden from views::ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void CreateItems();
  void UpdateFromScreenTrayItem();

 protected:
  views::ImageView* icon() { return icon_; }
  views::Label* label() { return label_; }
  views::Button* stop_button() { return stop_button_; }

 private:
  // The controller for this view. May be null.
  ScreenTrayItem* screen_tray_item_;
  views::ImageView* icon_;
  views::Label* label_;
  views::Button* stop_button_;
  base::string16 label_text_;
  base::string16 stop_button_text_;

  DISALLOW_COPY_AND_ASSIGN(ScreenStatusView);
};

class ScreenNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit ScreenNotificationDelegate(ScreenTrayItem* screen_tray);

  // message_center::NotificationDelegate overrides:
  void ButtonClick(int button_index) override;

 protected:
  ~ScreenNotificationDelegate() override;

 private:
  ScreenTrayItem* screen_tray_;

  DISALLOW_COPY_AND_ASSIGN(ScreenNotificationDelegate);
};

}  // namespace tray

// The base tray item for screen capture and screen sharing. The
// Start method brings up a notification and a tray item, and the user
// can stop the screen capture/sharing by pressing the stop button.
class ASH_EXPORT ScreenTrayItem : public SystemTrayItem {
 public:
  ScreenTrayItem(SystemTray* system_tray, UmaType uma_type);
  ~ScreenTrayItem() override;

  tray::ScreenTrayView* tray_view() { return tray_view_; }

  tray::ScreenStatusView* default_view() { return default_view_; }
  void set_default_view(tray::ScreenStatusView* default_view) {
    default_view_ = default_view;
  }

  bool is_started() const { return is_started_; }
  void set_is_started(bool is_started) { is_started_ = is_started; }

  void Update();
  void Start(const base::Closure& stop_callback);
  void Stop();

  // Creates or updates the notification for the tray item.
  virtual void CreateOrUpdateNotification() = 0;

  // Returns the id of the notification for the tray item.
  virtual std::string GetNotificationId() = 0;

  // Called after Stop() is invoked from the default view.
  virtual void RecordStoppedFromDefaultViewMetric() = 0;

  // Called after Stop() is invoked from the notification view.
  virtual void RecordStoppedFromNotificationViewMetric() = 0;

  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(LoginStatus status) override;
  views::View* CreateDefaultView(LoginStatus status) override = 0;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;

 private:
  tray::ScreenTrayView* tray_view_;
  tray::ScreenStatusView* default_view_;
  bool is_started_;
  base::Closure stop_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScreenTrayItem);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_SCREEN_SECURITY_SCREEN_TRAY_ITEM_H_
