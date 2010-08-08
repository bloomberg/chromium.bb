// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_APPLY_UPDATES_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_APPLY_UPDATES_COMMAND_H_
#pragma once

#include "chrome/browser/sync/engine/model_changing_syncer_command.h"

namespace syncable {
class WriteTransaction;
class MutableEntry;
class Id;
}

namespace browser_sync {

class ApplyUpdatesCommand : public ModelChangingSyncerCommand {
 public:
  ApplyUpdatesCommand();
  virtual ~ApplyUpdatesCommand();

  // ModelChangingSyncerCommand implementation.
  virtual void ModelChangingExecuteImpl(sessions::SyncSession* session);

 private:
  DISALLOW_COPY_AND_ASSIGN(ApplyUpdatesCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_APPLY_UPDATES_COMMAND_H_
