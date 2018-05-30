// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_H_
#define COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_H_

#include <memory>

#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {

// TODO(vitaliii): Don't inherit from ModelTypeSyncBridge, but add
// GetControllerDelegateOnUIThread() method instead.
class ConsentSyncBridge : public ModelTypeSyncBridge {
 public:
  explicit ConsentSyncBridge(
      std::unique_ptr<ModelTypeChangeProcessor> change_processor);
  ~ConsentSyncBridge() override = default;

  virtual void RecordConsent(
      std::unique_ptr<sync_pb::UserConsentSpecifics> specifics) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_H_
