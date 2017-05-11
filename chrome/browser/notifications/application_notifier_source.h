// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_APPLICATION_NOTIFIER_SOURCE_H_
#define CHROME_BROWSER_NOTIFICATIONS_APPLICATION_NOTIFIER_SOURCE_H_

#include "chrome/browser/notifications/notifier_source.h"
#include "chrome/browser/ui/app_icon_loader_delegate.h"

class AppIconLoader;

class ApplicationNotifierSource : public NotifierSource,
                                  public AppIconLoaderDelegate {
 public:
  explicit ApplicationNotifierSource(Observer* observer);
  ~ApplicationNotifierSource() override;

  std::vector<std::unique_ptr<message_center::Notifier>> GetNotifierList(
      Profile* profile) override;

  void SetNotifierEnabled(Profile* profile,
                          const message_center::Notifier& notifier,
                          bool enabled) override;

  message_center::NotifierId::NotifierType GetNotifierType() override;

 private:
  // Overridden from AppIconLoaderDelegate.
  void OnAppImageUpdated(const std::string& id,
                         const gfx::ImageSkia& image) override;

  std::unique_ptr<AppIconLoader> app_icon_loader_;

  // Lifetime of parent must be longer than the source.
  Observer* observer_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_APPLICATION_NOTIFIER_SOURCE_H_
