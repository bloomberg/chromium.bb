// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/app_icon_loader.h"
#include "chrome/common/content_settings.h"
#include "ui/message_center/notifier_settings.h"

class CancelableTaskTracker;

namespace chrome {
struct FaviconImageResult;
}

// The class to bridge between the settings UI of notifiers and the preference
// storage.
class MessageCenterSettingsController
    : public message_center::NotifierSettingsProvider,
      public extensions::AppIconLoader::Delegate {
 public:
  MessageCenterSettingsController();
  virtual ~MessageCenterSettingsController();

  // Overridden from message_center::NotifierSettingsProvider.
  virtual void AddObserver(
      message_center::NotifierSettingsObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      message_center::NotifierSettingsObserver* observer) OVERRIDE;
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
                       const chrome::FaviconImageResult& favicon_result);

  // The views displaying notifier settings.
  ObserverList<message_center::NotifierSettingsObserver> observers_;

  // The task tracker for loading favicons.
  scoped_ptr<CancelableTaskTracker> favicon_tracker_;

  scoped_ptr<extensions::AppIconLoader> app_icon_loader_;

  std::map<string16, ContentSettingsPattern> patterns_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterSettingsController);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
