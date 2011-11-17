// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LEGACY_WINDOW_MANAGER_INITIAL_BROWSER_WINDOW_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LEGACY_WINDOW_MANAGER_INITIAL_BROWSER_WINDOW_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class NotificationDetails;
class NotificationSource;
}

namespace chromeos {

// The old X window manager for Chrome OS touches a file under /var/run/state
// when it sees the first Chrome browser window get mapped post-login.  Tests
// then watch for this file to determine when login is completed.  There's no X
// window manager running for Aura builds, so we make Chrome replicate this
// behavior itself.
//
// TODO(derat): Once Aura is fully in use in Chrome OS, remove this and make
// tests instead use pyauto to communicate with Chrome.
class InitialBrowserWindowObserver : public content::NotificationObserver {
 public:
  InitialBrowserWindowObserver();
  virtual ~InitialBrowserWindowObserver() {}

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InitialBrowserWindowObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LEGACY_WINDOW_MANAGER_INITIAL_BROWSER_WINDOW_OBSERVER_H_
