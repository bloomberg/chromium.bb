// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This used to do a lot of TLS-based management of multiple Directory objects.
// We now can access Directory objects from any thread for general purpose
// operations and we only ever have one Directory, so this class isn't doing
// anything too fancy besides keeping calling and access conventions the same
// for now.
// TODO(timsteele): We can probably nuke this entire class and use raw
// Directory objects everywhere.
#ifndef CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_MANAGER_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_MANAGER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/sync/syncable/dir_open_result.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/cryptographer.h"
#include "chrome/common/deprecated/event_sys.h"

namespace sync_api { class BaseTransaction; }

namespace syncable {

struct DirectoryManagerEvent {
  enum {
    CLOSED,
    CLOSED_ALL,
    SHUTDOWN,
  } what_happened;
  std::string dirname;
  typedef DirectoryManagerEvent EventType;
  static inline bool IsChannelShutdownEvent(const EventType& event) {
    return SHUTDOWN == event.what_happened;
  }
};

DirectoryManagerEvent DirectoryManagerShutdownEvent();

class DirectoryManager {
 public:
  typedef EventChannel<DirectoryManagerEvent> Channel;

  // root_path specifies where db is stored.
  explicit DirectoryManager(const FilePath& root_path);
  virtual ~DirectoryManager();

  static const FilePath GetSyncDataDatabaseFilename();
  const FilePath GetSyncDataDatabasePath() const;

  // Opens a directory.  Returns false on error.
  // Name parameter is the the user's login,
  // MUST already have been converted to a common case.
  bool Open(const std::string& name);

  // Marks a directory as closed.  It might take a while until all the
  // file handles and resources are freed by other threads.
  void Close(const std::string& name);

  // Should be called at App exit.
  void FinalSaveChangesForAll();

  // Gets the list of currently open directory names.
  typedef std::vector<std::string> DirNames;
  void GetOpenDirectories(DirNames* result);

  Channel* channel() const { return channel_; }

  browser_sync::Cryptographer* cryptographer() const {
    return cryptographer_.get();
  }

 protected:
  DirOpenResult OpenImpl(const std::string& name, const FilePath& path,
                         bool* was_open);

  // Helpers for friend class ScopedDirLookup:
  friend class ScopedDirLookup;

  const FilePath root_path_;

  // protects managed_directory_
  base::Lock lock_;
  Directory* managed_directory_;

  Channel* const channel_;

  scoped_ptr<browser_sync::Cryptographer> cryptographer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DirectoryManager);
};


class ScopedDirLookup {
 public:
  ScopedDirLookup(DirectoryManager* dirman, const std::string& name);
  ~ScopedDirLookup();

  inline bool good() {
    good_checked_ = true;
    return good_;
  }
  Directory* operator -> () const;
  operator Directory* () const;

 protected:  // Don't allow creation on heap, except by sync API wrapper.
  friend class sync_api::BaseTransaction;
  void* operator new(size_t size) { return (::operator new)(size); }

  Directory* dir_;
  bool good_;
  // Ensure that the programmer checks good before using the ScopedDirLookup.
  // This member should can be removed if it ever shows up in profiling
  bool good_checked_;
  DirectoryManager* const dirman_;
};

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_MANAGER_H_
