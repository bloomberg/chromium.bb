// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_chromeos.h"

#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/menu_model_adapter.h"
#include "views/controls/menu/menu_runner.h"
#include "views/controls/menu/view_menu_delegate.h"

class StatusIconChromeOS::NotificationTrayButton
    : public StatusAreaButton,
      public views::MenuDelegate,
      public views::ViewMenuDelegate {
 public:
  NotificationTrayButton(StatusIconChromeOS* status_icon,
                         StatusAreaButton::Delegate* delegate)
      : StatusAreaButton(delegate, this),
        status_icon_(status_icon) {
  }
  virtual ~NotificationTrayButton() {}

  void SetImage(const SkBitmap& image) {
    SetIcon(image);
    SetVisible(true);
    PreferredSizeChanged();
    SchedulePaint();
  }

  void Hide() {
    SetVisible(false);
    SchedulePaint();
  }

  // views::MenuButton overrides.
  virtual bool Activate() OVERRIDE {
    // All tray buttons are removed on status icon destruction.
    // This should never be called afterwards.
    bool retval = views::MenuButton::Activate();
    status_icon_->Clicked();
    return retval;
  }

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt)
      OVERRIDE {
    views::MenuRunner* menu_runner = status_icon_->menu_runner();
    if (!menu_runner)
      return;

    gfx::Point screen_location;
    views::View::ConvertPointToScreen(source, &screen_location);
    gfx::Rect bounds(screen_location, source->size());
    if (menu_runner->RunMenuAt(
        source->GetWidget()->GetTopLevelWidget(), this, bounds,
        views::MenuItemView::TOPRIGHT, views::MenuRunner::HAS_MNEMONICS) ==
        views::MenuRunner::MENU_DELETED)
      return;
  }

 private:
  StatusIconChromeOS* status_icon_;
};

StatusIconChromeOS::StatusIconChromeOS()
    : last_image_(new SkBitmap()) {
  // This class adds a tray icon in the status area of all browser windows
  // in Chrome OS. This is the closest we can get to a system tray icon.
  for (BrowserList::BrowserVector::const_iterator it = BrowserList::begin();
      it != BrowserList::end(); ++it) {
    AddIconToBrowser(*it);
  }

  // Listen to all browser open/close events to keep our map up to date.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                 content::NotificationService::AllSources());
}

StatusIconChromeOS::~StatusIconChromeOS() {
  for (TrayButtonMap::const_iterator it = tray_button_map_.begin();
      it != tray_button_map_.end(); ++it) {
    it->first->RemoveTrayButton(it->second);
  }
}

void StatusIconChromeOS::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_WINDOW_READY: {
      Browser* browser = content::Source<Browser>(source).ptr();
      AddIconToBrowser(browser);
      break;
    }

    case chrome::NOTIFICATION_BROWSER_CLOSED: {
      chromeos::BrowserView* browser_view =
          chromeos::BrowserView::GetBrowserViewForBrowser(
          content::Source<Browser>(source).ptr());
      DCHECK(browser_view);
      tray_button_map_.erase(browser_view);
      break;
    }

    default:
      NOTREACHED();
  }
}

void StatusIconChromeOS::AddIconToBrowser(Browser* browser) {
  chromeos::BrowserView* browser_view =
      chromeos::BrowserView::GetBrowserViewForBrowser(browser);
  DCHECK(browser_view);

  if (tray_button_map_.find(browser_view) != tray_button_map_.end()) {
    NOTREACHED();
    return;
  }

  NotificationTrayButton* tray_button =
      new NotificationTrayButton(this, browser_view);
  tray_button_map_[browser_view] = tray_button;
  browser_view->AddTrayButton(tray_button, false);
  if (!last_image_->empty())
    tray_button->SetImage(*last_image_.get());
}

void StatusIconChromeOS::SetImage(const SkBitmap& image) {
  if (image.isNull())
    return;

  for (TrayButtonMap::const_iterator it = tray_button_map_.begin();
      it != tray_button_map_.end(); ++it) {
    it->second->SetImage(image);
  }
  *last_image_.get() = image;
}

void StatusIconChromeOS::SetPressedImage(const SkBitmap& image) {
  // Not supported. Chrome OS shows the context menu on left clicks.
}

void StatusIconChromeOS::SetToolTip(const string16& tool_tip) {
  for (TrayButtonMap::const_iterator it = tray_button_map_.begin();
      it != tray_button_map_.end(); ++it) {
    it->second->SetTooltipText(tool_tip);
  }
}

void StatusIconChromeOS::DisplayBalloon(const SkBitmap& icon,
                                        const string16& title,
                                        const string16& contents) {
  notification_.DisplayBalloon(icon, title, contents);
}

void StatusIconChromeOS::Clicked() {
  // Chrome OS shows the context menu on left button clicks, and this event
  // should only be dispatched when the click doesn't show the context menu.
  if (!menu_runner_.get())
    DispatchClickEvent();
}

StatusAreaButton* StatusIconChromeOS::GetButtonForBrowser(Browser* browser) {
  DCHECK(browser);
  chromeos::BrowserView* browser_view =
      chromeos::BrowserView::GetBrowserViewForBrowser(browser);
  DCHECK(browser_view);

  if (tray_button_map_.find(browser_view) == tray_button_map_.end()) {
    NOTREACHED();
    return NULL;
  }

  return tray_button_map_[browser_view];
}

void StatusIconChromeOS::UpdatePlatformContextMenu(ui::MenuModel* menu) {
  // If no items are passed, blow away our context menu.
  if (!menu) {
    context_menu_adapter_.reset();
    menu_runner_.reset();
    return;
  }

  // Create context menu with the new contents.
  views::MenuModelAdapter* adapter = new views::MenuModelAdapter(menu);
  context_menu_adapter_.reset(adapter);
  views::MenuItemView* menu_view = new views::MenuItemView(adapter);
  adapter->BuildMenu(menu_view);

  // menu_runner_ takes ownership of menu.
  menu_runner_.reset(new views::MenuRunner(menu_view));
}
