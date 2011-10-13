// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SEARCH_ENGINE_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_SEARCH_ENGINE_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/frontend_data_type_controller.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

namespace browser_sync {

class SearchEngineDataTypeController : public FrontendDataTypeController,
                                       public NotificationObserver {
 public:
  SearchEngineDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);
  virtual ~SearchEngineDataTypeController();

  // FrontendDataTypeController implementation.
  virtual syncable::ModelType type() const OVERRIDE;

  // NotificationObserver interface.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  // FrontendDataTypeController implementations.
  virtual bool StartModels() OVERRIDE;
  virtual void CleanUpState() OVERRIDE;
  virtual void CreateSyncComponents() OVERRIDE;
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;
  virtual void RecordAssociationTime(base::TimeDelta time) OVERRIDE;
  virtual void RecordStartFailure(StartResult result) OVERRIDE;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SEARCH_ENGINE_DATA_TYPE_CONTROLLER_H__
