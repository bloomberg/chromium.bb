// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/timer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;
namespace content {
class RenderProcessHost;
}

// We use this class to destroy the off the record profile so that we can make
// sure it gets done asynchronously after all render process hosts are gone.
class ProfileDestroyer
    : public content::NotificationObserver,
      public base::RefCounted<ProfileDestroyer> {
 public:
  static void DestroyProfileWhenAppropriate(Profile* const profile);
  static void DestroyOffTheRecordProfileNow(Profile* const profile);

 private:
  friend class base::RefCounted<ProfileDestroyer>;

  ProfileDestroyer(
      Profile* const profile,
      const std::vector<content::RenderProcessHost*>& hosts);
  virtual ~ProfileDestroyer();

  // content::NotificationObserver override.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
  // Called by the timer to cancel the pending detruction and do it now.
  void DestroyProfile();

  // Fetch the list of render process hosts that still refer to the profile.
  // Return true if we found at least one, false otherwise.
  static bool GetHostsForProfile(
      Profile* const profile,
      std::vector<content::RenderProcessHost*> *hosts);

  // We need access to all pending destroyers so we can cancel them.
  static std::vector<ProfileDestroyer*>* pending_destroyers_;

  // We register for render process host termination.
  content::NotificationRegistrar registrar_;

  // We don't want to wait forever, so we have a cancellation timer.
  base::Timer timer_;

  // Used to count down the number of render process host left.
  uint32 num_hosts_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDestroyer);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_
