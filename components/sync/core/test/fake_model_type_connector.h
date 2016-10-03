// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_TEST_FAKE_MODEL_TYPE_CONNECTOR_H_
#define COMPONENTS_SYNC_CORE_TEST_FAKE_MODEL_TYPE_CONNECTOR_H_

#include "components/sync/core/model_type_connector.h"

namespace syncer_v2 {

// A no-op implementation of ModelTypeConnector for testing.
class FakeModelTypeConnector : public ModelTypeConnector {
 public:
  FakeModelTypeConnector();
  ~FakeModelTypeConnector() override;

  void ConnectType(
      syncer::ModelType type,
      std::unique_ptr<ActivationContext> activation_context) override;
  void DisconnectType(syncer::ModelType type) override;
};

}  // namespace syncer_v2

#endif  // COMPONENTS_SYNC_CORE_TEST_FAKE_MODEL_TYPE_CONNECTOR_H_
