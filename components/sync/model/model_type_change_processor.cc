// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_change_processor.h"

#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace syncer {

// static
std::unique_ptr<ModelTypeChangeProcessor> ModelTypeChangeProcessor::Create(
    const base::RepeatingClosure& dump_stack,
    ModelType type,
    ModelTypeSyncBridge* bridge) {
  return std::make_unique<ClientTagBasedModelTypeProcessor>(
      type, bridge, dump_stack, CommitOnlyTypes().Has(type));
}

ModelTypeChangeProcessor::ModelTypeChangeProcessor() {}
ModelTypeChangeProcessor::~ModelTypeChangeProcessor() {}

}  // namespace syncer
