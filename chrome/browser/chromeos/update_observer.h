// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UPDATE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_UPDATE_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/chromeos/cros/update_library.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"

class Profile;

namespace chromeos {

// The update observer displays a system notification when the an update is
// available.

class UpdateObserver : public UpdateLibrary::Observer {
 public:
  explicit UpdateObserver(Profile* profile);
  virtual ~UpdateObserver();

 private:
  virtual void UpdateStatusChanged(UpdateLibrary* library);

  SystemNotification notification_;

  DISALLOW_COPY_AND_ASSIGN(UpdateObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UPDATE_OBSERVER_H_

