// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_NOTIFICATION_VIEW_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_NOTIFICATION_VIEW_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/slide_out_view.h"

namespace views {
class ImageView;
}

namespace ash {
class SystemTrayItem;

// A view for closable notification views, laid out like:
//  -------------------
// | icon  contents  x |
//  ----------------v--
// The close button will call OnClose() when clicked.
class TrayNotificationView : public views::SlideOutView,
                             public views::ButtonListener {
 public:
  // If icon_id is 0, no icon image will be set.
  TrayNotificationView(SystemTrayItem* owner, int icon_id);
  ~TrayNotificationView() override;

  // InitView must be called once with the contents to be displayed.
  void InitView(views::View* contents);

  // Replaces the contents view.
  void UpdateView(views::View* new_contents);

  // Autoclose timer operations.
  void StartAutoCloseTimer(int seconds);
  void StopAutoCloseTimer();
  void RestartAutoCloseTimer();

  // Overridden from ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from views::View.
  bool OnMousePressed(const ui::MouseEvent& event) override;

  // Overridden from ui::EventHandler.
  void OnGestureEvent(ui::GestureEvent* event) override;

 protected:
  // Called when the close button is pressed. Does nothing by default.
  virtual void OnClose();
  // Called when the notification is clicked on. Does nothing by default.
  virtual void OnClickAction();

  // Overridden from views::SlideOutView.
  void OnSlideOut() override;

  SystemTrayItem* owner() { return owner_; }

 private:
  void HandleClose();
  void HandleClickAction();

  SystemTrayItem* owner_;
  int icon_id_;
  views::ImageView* icon_;

  int autoclose_delay_;
  base::OneShotTimer autoclose_;

  DISALLOW_COPY_AND_ASSIGN(TrayNotificationView);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_NOTIFICATION_VIEW_H_
