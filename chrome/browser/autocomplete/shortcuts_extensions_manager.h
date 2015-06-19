// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_EXTENSIONS_MANAGER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_EXTENSIONS_MANAGER_H_

#include "base/supports_user_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

// This class manages the removal of shortcuts associated with an extension when
// that extension is unloaded.
class ShortcutsExtensionsManager : public base::SupportsUserData::Data,
                                   public content::NotificationObserver {
 public:
  explicit ShortcutsExtensionsManager(Profile* profile);
  ~ShortcutsExtensionsManager() override;

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  Profile* profile_;
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutsExtensionsManager);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_EXTENSIONS_MANAGER_H_
