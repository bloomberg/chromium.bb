// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/system/user/login_status.h"
#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#include <vector>

namespace ash {

class AudioObserver;
class BrightnessObserver;
class DateFormatObserver;
class NetworkObserver;
class PowerStatusObserver;
class UpdateObserver;

class SystemTrayItem;

namespace internal {
class SystemTrayBubble;
}

class ASH_EXPORT SystemTray : public views::View,
                              public views::Widget::Observer {
 public:
  SystemTray();
  virtual ~SystemTray();

  // Adds a new item in the tray.
  void AddTrayItem(SystemTrayItem* item);

  // Removes an existing tray item.
  void RemoveTrayItem(SystemTrayItem* item);

  // Shows the default view of all items.
  void ShowDefaultView();

  // Shows details of a particular item. If |close_delay_in_seconds| is
  // non-zero, then the view is automatically closed after the specified time.
  void ShowDetailedView(SystemTrayItem* item,
                        int close_delay_in_seconds,
                        bool activate);

  // Updates the items when the login status of the system changes.
  void UpdateAfterLoginStatusChange(user::LoginStatus login_status);

  const ScopedVector<SystemTrayItem>& items() const { return items_; }

  AudioObserver* audio_observer() const {
    return audio_observer_;
  }
  BrightnessObserver* brightness_observer() const {
    return brightness_observer_;
  }
  DateFormatObserver* date_format_observer() const {
    return date_format_observer_;
  }
  NetworkObserver* network_observer() const {
    return network_observer_;
  }
  PowerStatusObserver* power_status_observer() const {
    return power_status_observer_;
  }
  UpdateObserver* update_observer() const {
    return update_observer_;
  }

 private:
  friend class Shell;

  void ShowItems(std::vector<SystemTrayItem*>& items,
                 bool details,
                 bool activate);

  // Overridden from views::View.
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;

  // Overridden from views::Widget::Observer.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  ScopedVector<SystemTrayItem> items_;

  // The container for all the tray views of the items.
  views::View* container_;

  // These observers are not owned by the tray.
  AudioObserver* audio_observer_;
  BrightnessObserver* brightness_observer_;
  DateFormatObserver* date_format_observer_;
  NetworkObserver* network_observer_;
  PowerStatusObserver* power_status_observer_;
  UpdateObserver* update_observer_;

  // The popup widget and the delegate.
  internal::SystemTrayBubble* bubble_;
  views::Widget* popup_;

  DISALLOW_COPY_AND_ASSIGN(SystemTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_H_
