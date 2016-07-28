// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_NUDGE_HANDLER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_NUDGE_HANDLER_H_

#include "base/compiler_specific.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_export.h"

namespace syncer {

class SYNC_EXPORT NudgeHandler {
 public:
  NudgeHandler();
  virtual ~NudgeHandler();

  virtual void NudgeForInitialDownload(syncer::ModelType type) = 0;
  virtual void NudgeForCommit(syncer::ModelType type) = 0;
  virtual void NudgeForRefresh(syncer::ModelType type) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_NUDGE_HANDLER_H_
