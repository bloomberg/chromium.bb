// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_DOWNLOAD_UPDATES_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_DOWNLOAD_UPDATES_COMMAND_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/syncer_command.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace sync_pb {
class EntitySpecifics;
}

namespace browser_sync {

// Pick a subset of the enabled datatypes, download updates for them from
// the server, place the result in the SyncSession for further processing.
//
// The main inputs to this operation are the last_download_timestamp state
// in the syncable::Directory, and the set of enabled types as indicated by
// the SyncSession. DownloadUpdatesCommand will fetch the enabled type
// or types having the smallest (oldest) value for last_download_timestamp.
// DownloadUpdatesCommand will request a download of those types and will
// store the server response in the SyncSession. Only one server request
// is performed per Execute operation. A loop that causes multiple Execute
// operations within a sync session can be found in the Syncer logic.
// When looping, the DownloadUpdatesCommand consumes the information stored
// by the StoreTimestampsCommand.
//
// In practice, DownloadUpdatesCommand should exhibit one of two behaviors.
// (a) If one or more datatypes has just been enabled, then they will have the
//     oldest last_download_timestamp value (0). DownloadUpdatesCommand will
//     choose to download updates for those types, and likely this will happen
//     for several invocations of DownloadUpdatesCommand. Once the newly
//     enabled types have caught up to the timestamp value of any previously
//     enabled timestamps, DownloadUpdatesCommand will do a fetch for those
//     datatypes. If nothing has changed on the server in the meantime,
//     then the timestamp value for the new and old datatypes will now match.
//     When that happens (and it eventually should), we have entered case (b).
// (b) The common case is for all enabled datatypes to have the same
//     last_download_timestamp value. This means that one server request
//     tells us whether there are updates available to any datatype.  When
//     the last_download_timestamp values for two datatypes is identical,
//     those datatypes will never be separately requested, and the values
//     will stay in lockstep indefinitely.
class DownloadUpdatesCommand : public SyncerCommand {
 public:
  DownloadUpdatesCommand();
  virtual ~DownloadUpdatesCommand();

  // SyncerCommand implementation.
  virtual void ExecuteImpl(sessions::SyncSession* session);

  void SetRequestedTypes(const syncable::ModelTypeBitSet& target_datatypes,
                         sync_pb::EntitySpecifics* filter_protobuf);

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadUpdatesCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_DOWNLOAD_UPDATES_COMMAND_H_

