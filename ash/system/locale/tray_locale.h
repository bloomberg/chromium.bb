// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_TRAY_LOCALE_H_
#define ASH_SYSTEM_LOCALE_TRAY_LOCALE_H_

#include <string>

#include "ash/system/locale/locale_observer.h"
#include "ash/system/tray/system_tray_item.h"

namespace ash {
namespace internal {

namespace tray {
class LocaleNotificationView;
}

class TrayLocale : public SystemTrayItem,
                   public LocaleObserver {
 public:
  explicit TrayLocale(SystemTray* system_tray);
  virtual ~TrayLocale();

 private:
  // Overridden from TrayImageItem.
  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE;
  virtual void DestroyNotificationView() OVERRIDE;

  // Overridden from LocaleObserver.
  virtual void OnLocaleChanged(LocaleObserver::Delegate* delegate,
                               const std::string& cur_locale,
                               const std::string& from_locale,
                               const std::string& to_locale) OVERRIDE;

  tray::LocaleNotificationView* notification_;
  LocaleObserver::Delegate* delegate_;
  std::string cur_locale_;
  std::string from_locale_;
  std::string to_locale_;

  DISALLOW_COPY_AND_ASSIGN(TrayLocale);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_TRAY_LOCALE_H_
