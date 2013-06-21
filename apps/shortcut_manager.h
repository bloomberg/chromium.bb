// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHORTCUT_MANAGER_H_
#define APPS_SHORTCUT_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/extension.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefService;
class Profile;

namespace apps {

// This class manages the installation of shortcuts for platform apps.
class ShortcutManager : public BrowserContextKeyedService,
                        public content::NotificationObserver {
 public:
  explicit ShortcutManager(Profile* profile);

  virtual ~ShortcutManager();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Checks if kShortcutsEnabled is set in prefs. If not, this sets it and
  // creates shortcuts for all apps.
  void OnceOffCreateShortcuts();

  void DeleteApplicationShortcuts(const extensions::Extension* extension);

  content::NotificationRegistrar registrar_;
  Profile* profile_;
  PrefService* prefs_;

  // Fields used when installing application shortcuts.
  base::WeakPtrFactory<ShortcutManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutManager);
};

}  // namespace apps

#endif  // APPS_SHORTCUT_MANAGER_H_
