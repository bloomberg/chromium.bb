// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"

namespace gfx {
class ImageSkia;
}

namespace message_center {
struct NotifierId;
}

class StatusIconObserver;

class StatusIcon {
 public:
  StatusIcon();
  virtual ~StatusIcon();

  // Sets the image associated with this status icon.
  virtual void SetImage(const gfx::ImageSkia& image) = 0;

  // Sets the hover text for this status icon. This is also used as the label
  // for the menu item which is created as a replacement for the status icon
  // click action on platforms that do not support custom click actions for the
  // status icon (e.g. Ubuntu Unity).
  virtual void SetToolTip(const base::string16& tool_tip) = 0;

  // Displays a notification balloon with the specified contents.
  // Depending on the platform it might not appear by the icon tray.
  virtual void DisplayBalloon(
      const gfx::ImageSkia& icon,
      const base::string16& title,
      const base::string16& contents,
      const message_center::NotifierId& notifier_id) = 0;

  // Set the context menu for this icon. The icon takes ownership of the passed
  // context menu. Passing NULL results in no menu at all.
  void SetContextMenu(std::unique_ptr<StatusIconMenuModel> menu);

  // Adds/Removes an observer for clicks on the status icon. If an observer is
  // registered, then left clicks on the status icon will result in the observer
  // being called, otherwise, both left and right clicks will display the
  // context menu (if any).
  void AddObserver(StatusIconObserver* observer);
  void RemoveObserver(StatusIconObserver* observer);

  // Returns true if there are registered click observers.
  bool HasObservers() const;

  // Dispatches a click event to the observers.
  void DispatchClickEvent();
#if defined(OS_WIN)
  void DispatchBalloonClickEvent();
#endif

  // Attempts to make the status icon directly visible on system UI.  Currently
  // this only applies to Windows, where status icons are hidden by default
  // inside an overflow window.
  // WARNING: This currently uses undocumented Windows APIs and spawns a worker
  // thread to do it.  Use sparingly.
  virtual void ForceVisible();

 protected:
  // Invoked after a call to SetContextMenu() to let the platform-specific
  // subclass update the native context menu based on the new model. If NULL is
  // passed, subclass should destroy the native context menu.
  virtual void UpdatePlatformContextMenu(StatusIconMenuModel* model) = 0;

 private:
  base::ObserverList<StatusIconObserver>::Unchecked observers_;

  // Context menu, if any.
  std::unique_ptr<StatusIconMenuModel> context_menu_contents_;

  DISALLOW_COPY_AND_ASSIGN(StatusIcon);
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_H_
