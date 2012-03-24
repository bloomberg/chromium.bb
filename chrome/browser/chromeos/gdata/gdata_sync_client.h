// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYNC_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYNC_CLIENT_H_
#pragma once

#include <queue>
#include <string>
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"

namespace gdata {

// The GDataSyncClient is used to synchronize pinned files on gdata and the
// cache on the local drive. The sync client works as follows.
//
// When the user pins files on gdata, this client is notified about the files
// that get pinned, and queues tasks and starts fetching these files in the
// background.
//
// When the user unpins files on gdata, this client is notified about the
// files that get unpinned, cancels tasks if these are still in the queue.
//
// If the user logs out before fetching of the pinned files is complete, this
// client resumes fetching operations next time the user logs in, based on
// the states left in the cache.
//
// TODO(satorux): This client should also upload pinned but dirty (locally
// edited) files to gdata. Will work on this once downloading is done.
// crosbug.com/27836.
//

// The interface class is defined to make GDataSyncClient mockable.
class GDataSyncClientInterface : public GDataFileSystem::Observer {
 public:
  // Initializes the GDataSyncClient.
  virtual void Initialize() = 0;

  virtual ~GDataSyncClientInterface() {}
};

// The production implementation of GDataSyncClientInterface.
class GDataSyncClient : public GDataSyncClientInterface {
 public:
  // |file_system| is used to access to the
  // cache (ex. store a file to the cache when the file is downloaded).
  explicit GDataSyncClient(GDataFileSystemInterface* file_system);
  virtual ~GDataSyncClient();

  // GDataSyncClientInterface overrides.
  virtual void Initialize() OVERRIDE;

  // GDataFileSystem::Observer overrides.
  virtual void OnCacheInitialized() OVERRIDE;
  virtual void OnFilePinned(const std::string& resource_id,
                            const std::string& md5) OVERRIDE;
  virtual void OnFileUnpinned(const std::string& resource_id,
                              const std::string& md5) OVERRIDE;

  // Starts scanning the pinned directory in the cache to collect
  // pinned-but-not-fetched files. |closure| is run on the calling thread
  // once the initial scan is complete.
  //
  // TODO(satorux): This function isn't used yet in the production code.
  // We should get notified about completion of the cache initialization, and
  // call this function.
  void StartInitialScan(const base::Closure& closure);

  // Returns the contents of |queue_|. Used only for testing.
  std::vector<std::string> GetResourceIdInQueueForTesting();

 private:
  // Called when the initial scan is complete. Receives the resource IDs of
  // pinned-but-not-fetched files as |resource_ids|. |closure| is run at the
  // end.
  void OnInitialScanComplete(const base::Closure& closure,
                             std::vector<std::string>* resource_ids);

  GDataFileSystemInterface* file_system_;

  // The queue of resource IDs used to fetch pinned-but-not-fetched files in
  // the background thread.
  std::queue<std::string> queue_;

  base::WeakPtrFactory<GDataSyncClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GDataSyncClient);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYNC_CLIENT_H_
