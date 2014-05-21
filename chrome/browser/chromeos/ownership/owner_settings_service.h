// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace chromeos {

// This class reloads owner key from profile NSS slots.
//
// TODO (ygorshenin@): move write path for device settings here
// (crbug.com/230018).
class OwnerSettingsService : public KeyedService,
                             public content::NotificationObserver {
 public:
  virtual ~OwnerSettingsService();

  // NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class OwnerSettingsServiceFactory;

  explicit OwnerSettingsService(Profile* profile);

  void ReloadOwnerKey();

  Profile* profile_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<OwnerSettingsService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_H_
