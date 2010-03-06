// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#ifndef CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_VIEW_H_

#include "app/gfx/path.h"
#include "app/menus/simple_menu_model.h"
#include "base/basictypes.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "views/controls/button/button.h"
#include "views/controls/label.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

namespace views {
class Menu2;
class MenuButton;
class TextButton;
}  // namespace views

class BalloonViewHost;
class NotificationDetails;
class NotificationSource;

namespace chromeos {

// A balloon view is the UI component for a notification panel.
class BalloonViewImpl : public BalloonView,
                        public views::View,
                        public views::ViewMenuDelegate,
                        public menus::SimpleMenuModel::Delegate,
                        public NotificationObserver,
                        public views::ButtonListener {
 public:
  BalloonViewImpl();
  ~BalloonViewImpl();

  // views::View interface.
  virtual void Layout();

  // BalloonView interface.
  void Show(Balloon* balloon);
  void Close(bool by_user);
  void RepositionToBalloon();
  gfx::Size GetSize() const;

 private:
  // views::View interface.
  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(1000, 1000);
  }

  // views::ViewMenuDelegate interface.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // views::ButtonListener interface.
  virtual void ButtonPressed(views::Button* sender, const views::Event&);

  // menus::SimpleMenuModel::Delegate interface.
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Initializes the options menu.
  void CreateOptionsMenu();

  // Do the delayed close work.
  void DelayedClose(bool by_user);

  // Non-owned pointer to the balloon which owns this object.
  Balloon* balloon_;

  // The renderer of the HTML contents. Pointer owned by the views hierarchy.
  BalloonViewHost* html_contents_;

  // The following factory is used to call methods at a later time.
  ScopedRunnableMethodFactory<BalloonViewImpl> method_factory_;

  // Pointer to sub-view is owned by the View sub-class.
  views::TextButton* close_button_;

  // Pointer to sub-view is owned by View class.
  views::Label* source_label_;

  // The options menu.
  scoped_ptr<menus::SimpleMenuModel> options_menu_contents_;
  scoped_ptr<views::Menu2> options_menu_menu_;
  views::MenuButton* options_menu_button_;

  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_VIEW_H_
