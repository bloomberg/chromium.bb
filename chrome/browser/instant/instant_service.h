// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_SERVICE_H_
#define CHROME_BROWSER_INSTANT_INSTANT_SERVICE_H_

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

// Tracks render process host IDs that are associated with Instant.
class InstantService : public ProfileKeyedService,
                       public content::NotificationObserver {
 public:
  InstantService();
  virtual ~InstantService();

  // Add, remove, and query RenderProcessHost IDs that are associated with
  // Instant processes.
  void AddInstantProcess(int process_id);
  bool IsInstantProcess(int process_id) const;

#if defined(UNIT_TEST)
  int GetInstantProcessCount() const {
    return process_ids_.size();
  }
#endif

 private:
  // Overridden from ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // The process ids associated with Instant processes.
  std::set<int> process_ids_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InstantService);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_SERVICE_H_
