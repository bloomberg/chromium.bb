// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/extensions/app_icon_loader.h"
#include "chrome/browser/history/history_types.h"
#include "ui/message_center/notifier_settings_view_delegate.h"

class CancelableTaskTracker;

namespace message_center {
class NotifierSettingsView;
}

// The class to bridge between the settings UI of notifiers and the preference
// storage.
class MessageCenterSettingsController
    : public message_center::NotifierSettingsViewDelegate,
      public extensions::AppIconLoader::Delegate {
 public:
  MessageCenterSettingsController();
  virtual ~MessageCenterSettingsController();

  // Shows a new settings dialog window with specified |context|. If there's
  // already an existing dialog, it raises the dialog instead of creating a new
  // one.
  void ShowSettingsDialog(gfx::NativeView context);

  // Overridden from message_center::NotifierSettingsViewDelegate.
  virtual void GetNotifierList(
      std::vector<message_center::Notifier*>* notifiers)
      OVERRIDE;
  virtual void SetNotifierEnabled(
      const message_center::Notifier& notifier,
      bool enabled) OVERRIDE;
  virtual void OnNotifierSettingsClosing() OVERRIDE;

  // Overridden from extensions::AppIconLoader::Delegate.
  virtual void SetAppImage(const std::string& id,
                           const gfx::ImageSkia& image) OVERRIDE;

 private:
  void OnFaviconLoaded(const GURL& url,
                       const history::FaviconImageResult& favicon_result);

  // The view displaying notifier settings. NULL if the settings are not
  // visible.
  message_center::NotifierSettingsView* settings_view_;

  // The task tracker for loading favicons.
  scoped_ptr<CancelableTaskTracker> favicon_tracker_;

  scoped_ptr<extensions::AppIconLoader> app_icon_loader_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterSettingsController);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
