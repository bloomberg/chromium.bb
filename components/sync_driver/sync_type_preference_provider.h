// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_TYPE_PREFERENCE_PROVIDER_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_TYPE_PREFERENCE_PROVIDER_H_

#include "sync/internal_api/public/base/model_type.h"

class SyncTypePreferenceProvider {
 public:
  virtual syncer::ModelTypeSet GetPreferredDataTypes() const = 0;

 protected:
  virtual ~SyncTypePreferenceProvider() {}
};

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_TYPE_PREFERENCE_PROVIDER_H_
