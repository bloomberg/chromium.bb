// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYNC_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYNC_CLIENT_H_
#pragma once

#include <deque>
#include <string>
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_source.h"

class Profile;
class PrefChangeRegistrar;

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
class GDataSyncClientInterface {
 public:
  // Initializes the GDataSyncClient.
  virtual void Initialize() = 0;

  virtual ~GDataSyncClientInterface() {}
};

// The production implementation of GDataSyncClientInterface.
class GDataSyncClient
    : public GDataSyncClientInterface,
      public GDataFileSystem::Observer,
      public chromeos::NetworkLibrary::NetworkManagerObserver,
      public content::NotificationObserver {
 public:
  // |profile| is used to access user preferences.
  // |file_system| is used access the
  // cache (ex. store a file to the cache when the file is downloaded).
  GDataSyncClient(Profile* profile,
                  GDataFileSystemInterface* file_system);
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
  void StartInitialScan(const base::Closure& closure);

  // Returns the contents of |queue_|. Used only for testing.
  const std::deque<std::string>& GetResourceIdsForTesting() const {
    return queue_;
  }

  // Adds the resource ID to the queue. Used only for testing.
  void AddResourceIdForTesting(const std::string& resource_id) {
    queue_.push_back(resource_id);
  }

  // Starts the fetch loop if it's not running.
  void StartFetchLoop();

 private:
  friend class GDataSyncClientTest;

  // Runs the fetch loop that fetches files in |queue_|. One file is fetched
  // at a time, rather than in parallel. The loop ends when the queue becomes
  // empty.
  void DoFetchLoop();

  // Returns true if we should stop the fetch loop.
  bool ShouldStopFetchLoop();

  // Called when the initial scan is complete. Receives the resource IDs of
  // pinned-but-not-fetched files as |resource_ids|. |closure| is run at the
  // end.
  void OnInitialScanComplete(const base::Closure& closure,
                             std::vector<std::string>* resource_ids);

  // Called when the file for |resource_id| is fetched.
  // Calls DoFetchLoop() to go back to the fetch loop.
  void OnFetchFileComplete(const std::string& resource_id,
                           base::PlatformFileError error,
                           const FilePath& local_path,
                           const std::string& ununsed_mime_type,
                           GDataFileType file_type);

  // chromeos::NetworkLibrary::NetworkManagerObserver override.
  virtual void OnNetworkManagerChanged(
      chromeos::NetworkLibrary* network_library) OVERRIDE;

  // content::NotificationObserver override.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
  Profile* profile_;
  GDataFileSystemInterface* file_system_;
  scoped_ptr<PrefChangeRegistrar> registrar_;

  // The queue of resource IDs used to fetch pinned-but-not-fetched files in
  // the background thread. Note that this class does not use a lock to
  // protect |queue_| as all methods touching |queue_| run on the UI thread.
  std::deque<std::string> queue_;

  // True if the fetch loop is running.
  bool fetch_loop_is_running_;

  base::WeakPtrFactory<GDataSyncClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GDataSyncClient);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYNC_CLIENT_H_
