// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_MANAGER_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/session_manager_client.h"

namespace chromeos {

// SessionManagerObserver is used to take actions per signals sent from the
// session manager.
class SessionManagerObserver : public SessionManagerClient::Observer {
 public:
  SessionManagerObserver();
  virtual ~SessionManagerObserver();

 private:
  // SessionManagerClient::Observer override.
  virtual void OwnerKeySet(bool success) OVERRIDE;
  // SessionManagerClient::Observer override.
  virtual void PropertyChangeComplete(bool success) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_MANAGER_OBSERVER_H_
