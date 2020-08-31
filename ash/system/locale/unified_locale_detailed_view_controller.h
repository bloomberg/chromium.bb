// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_UNIFIED_LOCALE_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_LOCALE_UNIFIED_LOCALE_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/system/unified/detailed_view_controller.h"
#include "base/macros.h"

namespace ash {

namespace tray {
class LocaleDetailedView;
}  // namespace tray

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of the locale detailed view in UnifiedSystemTray.
class UnifiedLocaleDetailedViewController : public DetailedViewController {
 public:
  explicit UnifiedLocaleDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedLocaleDetailedViewController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;
  base::string16 GetAccessibleName() const override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  tray::LocaleDetailedView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedLocaleDetailedViewController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_UNIFIED_LOCALE_DETAILED_VIEW_CONTROLLER_H_
