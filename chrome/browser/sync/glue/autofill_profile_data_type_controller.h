// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
#pragma once

#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_factory.h"

namespace browser_sync {

class AutofillProfileDataTypeController : public AutofillDataTypeController {
 public:
  AutofillProfileDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile);
  virtual ~AutofillProfileDataTypeController();

  virtual syncable::ModelType type() const;

 protected:
   virtual void CreateSyncComponents();
   virtual void RecordUnrecoverableError(
       const tracked_objects::Location& from_here,
       const std::string& message);
   virtual void RecordAssociationTime(base::TimeDelta time);
   virtual void RecordStartFailure(StartResult result);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
