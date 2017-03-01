// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_NOTIFICATION_VIEW_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_NOTIFICATION_VIEW_H_

#include "ui/views/view.h"

namespace views {
class ImageView;
}

namespace ash {

// A view for closable notification views, laid out like:
//  -------------------
// | icon  contents  x |
//  ----------------v--
// The close button will call OnClose() when clicked.
class TrayNotificationView : public views::View {
 public:
  // If icon_id is 0, no icon image will be set.
  explicit TrayNotificationView(int icon_id);
  ~TrayNotificationView() override;

  // InitView must be called once with the contents to be displayed.
  void InitView(views::View* contents);

 private:
  int icon_id_;
  views::ImageView* icon_;

  DISALLOW_COPY_AND_ASSIGN(TrayNotificationView);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_NOTIFICATION_VIEW_H_
