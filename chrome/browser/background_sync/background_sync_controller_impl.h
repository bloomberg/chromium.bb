// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_IMPL_H_

#include "content/public/browser/background_sync_controller.h"

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_thread.h"

namespace rappor {
class RapporService;
}

class Profile;

class BackgroundSyncControllerImpl : public content::BackgroundSyncController,
                                     public KeyedService {
 public:
  explicit BackgroundSyncControllerImpl(Profile* profile);
  ~BackgroundSyncControllerImpl() override;

  // content::BackgroundSyncController overrides.
  void NotifyBackgroundSyncRegistered(const GURL& origin) override;

#if defined(OS_ANDROID)
  void LaunchBrowserWhenNextOnline(
      const content::BackgroundSyncManager* registrant,
      bool launch_when_next_online) override;
#endif

 protected:
  // Virtual for testing.
  virtual rappor::RapporService* GetRapporService();

 private:
  Profile* profile_;  // This object is owned by profile_.

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncControllerImpl);
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_IMPL_H_
