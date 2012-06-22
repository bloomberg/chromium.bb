// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SESSION_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_SESSION_DATA_TYPE_CONTROLLER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/frontend_data_type_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace browser_sync {

class SessionModelAssociator;

class SessionDataTypeController : public FrontendDataTypeController,
                                  public content::NotificationObserver {
 public:
  SessionDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);

  SessionModelAssociator* GetModelAssociator();

  // FrontendDataTypeController implementation.
  virtual syncable::ModelType type() const OVERRIDE;

  // NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  virtual ~SessionDataTypeController();

  // FrontendDataTypeController implementations.
  virtual bool StartModels() OVERRIDE;
  virtual void CleanUpState() OVERRIDE;
  // Datatype specific creation of sync components.
  virtual void CreateSyncComponents() OVERRIDE;

  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SessionDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SESSION_DATA_TYPE_CONTROLLER_H_

