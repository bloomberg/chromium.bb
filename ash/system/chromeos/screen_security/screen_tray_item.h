// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_SCREEN_TRAY_ITEM_H_
#define ASH_SYSTEM_CHROMEOS_SCREEN_TRAY_ITEM_H_

#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_popup_label_button.h"
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
  ScreenTrayView(ScreenTrayItem* screen_tray_item, int icon_id);
  virtual ~ScreenTrayView();

  void Update();

 private:
  ScreenTrayItem* screen_tray_item_;

  DISALLOW_COPY_AND_ASSIGN(ScreenTrayView);
};

class ScreenStatusView : public views::View,
                         public views::ButtonListener {
 public:
  ScreenStatusView(ScreenTrayItem* screen_tray_item,
                   int icon_id,
                   const base::string16& label_text,
                   const base::string16& stop_button_text);
  virtual ~ScreenStatusView();

  // Overridden from views::View.
  virtual void Layout() OVERRIDE;

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  void CreateItems();
  void Update();

 private:
  ScreenTrayItem* screen_tray_item_;
  views::ImageView* icon_;
  views::Label* label_;
  TrayPopupLabelButton* stop_button_;
  int icon_id_;
  base::string16 label_text_;
  base::string16 stop_button_text_;

  DISALLOW_COPY_AND_ASSIGN(ScreenStatusView);
};

class ScreenNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit ScreenNotificationDelegate(ScreenTrayItem* screen_tray);

  // message_center::NotificationDelegate overrides:
  virtual void Display() OVERRIDE;
  virtual void Error() OVERRIDE;
  virtual void Close(bool by_user) OVERRIDE;
  virtual void Click() OVERRIDE;
  virtual void ButtonClick(int button_index) OVERRIDE;

 protected:
  virtual ~ScreenNotificationDelegate();

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
  explicit ScreenTrayItem(SystemTray* system_tray);
  virtual ~ScreenTrayItem();

  tray::ScreenTrayView* tray_view() { return tray_view_; }
  void set_tray_view(tray::ScreenTrayView* tray_view) {
    tray_view_ = tray_view;
  }

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

  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE = 0;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE = 0;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void UpdateAfterShelfAlignmentChange(
      ShelfAlignment alignment) OVERRIDE;

 private:
  tray::ScreenTrayView* tray_view_;
  tray::ScreenStatusView* default_view_;
  bool is_started_;
  base::Closure stop_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScreenTrayItem);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_SCREEN_TRAY_ITEM_H_
