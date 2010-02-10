// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/bookmark_model_associator.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory_impl.h"
#include "chrome/common/chrome_switches.h"

ProfileSyncFactoryImpl::ProfileSyncFactoryImpl(
    Profile* profile,
    CommandLine* command_line)
    : profile_(profile), command_line_(command_line) {
}

ProfileSyncService* ProfileSyncFactoryImpl::CreateProfileSyncService() {
  ProfileSyncService* pss = new ProfileSyncService(profile_);

  // Bookmark sync is enabled by default.  Register unless explicitly disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncBookmarks)) {
    pss->RegisterDataTypeController(
        new browser_sync::BookmarkDataTypeController(this, profile_, pss));
  }
  return pss;
}

ProfileSyncFactory::BookmarkComponents
ProfileSyncFactoryImpl::CreateBookmarkComponents(
    ProfileSyncService* profile_sync_service) {
  browser_sync::BookmarkModelAssociator* model_associator =
      new browser_sync::BookmarkModelAssociator(profile_sync_service);
  browser_sync::BookmarkChangeProcessor* change_processor =
      new browser_sync::BookmarkChangeProcessor(model_associator,
                                                profile_sync_service);
  return BookmarkComponents(model_associator, change_processor);
}
