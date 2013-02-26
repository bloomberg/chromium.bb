// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_BOOKMARK_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_BOOKMARK_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/sync/glue/frontend_data_type_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace browser_sync {

// A class that manages the startup and shutdown of bookmark sync.
class BookmarkDataTypeController : public FrontendDataTypeController,
                                   public content::NotificationObserver,
                                   public BaseBookmarkModelObserver {
 public:
  BookmarkDataTypeController(ProfileSyncComponentsFactory* profile_sync_factory,
                             Profile* profile,
                             ProfileSyncService* sync_service);

  // FrontendDataTypeController interface.
  virtual syncer::ModelType type() const OVERRIDE;

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  virtual ~BookmarkDataTypeController();

  // FrontendDataTypeController interface.
  virtual bool StartModels() OVERRIDE;
  virtual void CleanUpState() OVERRIDE;
  virtual void CreateSyncComponents() OVERRIDE;

  // BaseBookmarkModelObserver interface.
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;

  // Helper that returns true iff both the bookmark model and the history
  // service have finished loading.
  bool DependentsLoaded();

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BOOKMARK_DATA_TYPE_CONTROLLER_H__
