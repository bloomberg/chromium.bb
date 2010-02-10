// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_H__

#include <utility>
#include "base/task.h"

class ProfileSyncService;

namespace browser_sync {
class AssociatorInterface;
class ChangeProcessor;
}

// Factory class for all profile sync related classes.
class ProfileSyncFactory {
 public:
  struct BookmarkComponents {
    browser_sync::AssociatorInterface* model_associator;
    browser_sync::ChangeProcessor* change_processor;
    BookmarkComponents(browser_sync::AssociatorInterface* ma,
                       browser_sync::ChangeProcessor* cp)
        : model_associator(ma), change_processor(cp) {}
  };

  virtual ~ProfileSyncFactory() {}

  // Instantiates and initializes a new ProfileSyncService.  Enabled
  // data types are registered with the service.  The return pointer
  // is owned by the caller.
  virtual ProfileSyncService* CreateProfileSyncService() = 0;

  // Instantiates both a model associator and change processor for the
  // bookmark data type.  Ideally we would have separate factory
  // methods for these components, but the bookmark implementation of
  // these are tightly coupled and must be created at the same time.
  // The pointers in the return struct are owned by the caller.
  virtual BookmarkComponents CreateBookmarkComponents(
      ProfileSyncService* profile_sync_service) = 0;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_H__
