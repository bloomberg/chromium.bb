// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_RESOLVE_CONFLICTS_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_RESOLVE_CONFLICTS_COMMAND_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/engine/model_changing_syncer_command.h"

namespace browser_sync {

class ResolveConflictsCommand : public ModelChangingSyncerCommand {
 public:
  ResolveConflictsCommand();
  virtual ~ResolveConflictsCommand();

  // ModelChangingSyncerCommand implementation.
  virtual void ModelChangingExecuteImpl(
      sessions::SyncSession* session) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResolveConflictsCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_RESOLVE_CONFLICTS_COMMAND_H_
