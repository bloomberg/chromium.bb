// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/directory_manager.h"

#include <map>
#include <set>
#include <iterator>

#include "base/logging.h"
#include "base/port.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/browser/sync/util/path_helpers.h"

namespace syncable {

static const PSTR_CHAR kSyncDataDatabaseFilename[] = PSTR("SyncData.sqlite3");

DirectoryManagerEvent DirectoryManagerShutdownEvent() {
  DirectoryManagerEvent event;
  event.what_happened = DirectoryManagerEvent::SHUTDOWN;
  return event;
}

// static
const PathString DirectoryManager::GetSyncDataDatabaseFilename() {
  return PathString(kSyncDataDatabaseFilename);
}

const PathString DirectoryManager::GetSyncDataDatabasePath() const {
  PathString path(root_path_);
  path.append(kSyncDataDatabaseFilename);
  return path;
}

DirectoryManager::DirectoryManager(const PathString& path)
    : root_path_(AppendSlash(path)),
      channel_(new Channel(DirectoryManagerShutdownEvent())),
      managed_directory_(NULL) {
  CHECK(0 == pthread_mutex_init(&mutex_, NULL));
}

DirectoryManager::~DirectoryManager() {
  DCHECK_EQ(managed_directory_, static_cast<Directory*>(NULL))
      << "Dir " << managed_directory_->name() << " not closed!";
  pthread_mutex_lock(&mutex_);
  delete channel_;
  pthread_mutex_unlock(&mutex_);
  CHECK(0 == pthread_mutex_destroy(&mutex_));
}

bool DirectoryManager::Open(const PathString& name) {
  bool was_open = false;
  const DirOpenResult result = OpenImpl(name,
      GetSyncDataDatabasePath(), &was_open);
  if (!was_open) {
    DirectoryManagerEvent event;
    event.dirname = name;
    if (syncable::OPENED == result) {
      event.what_happened = DirectoryManagerEvent::OPENED;
    } else {
      event.what_happened = DirectoryManagerEvent::OPEN_FAILED;
      event.error = result;
    }
    channel_->NotifyListeners(event);
  }
  return syncable::OPENED == result;
}

// Opens a directory.  Returns false on error.
DirOpenResult DirectoryManager::OpenImpl(const PathString& name,
                                         const PathString& path,
                                         bool* was_open) {
  pthread_mutex_lock(&mutex_);
  // Check to see if it's already open.
  bool opened = false;
  if (managed_directory_) {
    DCHECK_EQ(ComparePathNames(name, managed_directory_->name()), 0)
        << "Can't open more than one directory.";
    opened = *was_open = true;
  }
  pthread_mutex_unlock(&mutex_);
  if (opened)
    return syncable::OPENED;
  // Otherwise, open it.

  Directory* dir = new Directory;
  const DirOpenResult result = dir->Open(path, name);
  if (syncable::OPENED == result) {
    pthread_mutex_lock(&mutex_);
    managed_directory_ = dir;
    pthread_mutex_unlock(&mutex_);
  } else {
    delete dir;
  }
  return result;
}

// Marks a directory as closed.  It might take a while until all the
// file handles and resources are freed by other threads.
void DirectoryManager::Close(const PathString& name) {
  // Erase from mounted and opened directory lists.
  pthread_mutex_lock(&mutex_);

  if (!managed_directory_ ||
      ComparePathNames(name, managed_directory_->name()) != 0) {
    // It wasn't open;
    pthread_mutex_unlock(&mutex_);
    return;
  }
  pthread_mutex_unlock(&mutex_);

  // Notify listeners.
  managed_directory_->channel()->NotifyListeners(DIRECTORY_CLOSED);
  DirectoryManagerEvent event = { DirectoryManagerEvent::CLOSED, name };
  channel_->NotifyListeners(event);

  delete managed_directory_;
  managed_directory_ = NULL;
}

// Marks all directories as closed.  It might take a while until all the
// file handles and resources are freed by other threads.
void DirectoryManager::CloseAllDirectories() {
  if (managed_directory_)
    Close(managed_directory_->name());
}

void DirectoryManager::FinalSaveChangesForAll() {
  pthread_mutex_lock(&mutex_);
  if (managed_directory_)
    managed_directory_->SaveChanges();
  pthread_mutex_unlock(&mutex_);
}

void DirectoryManager::GetOpenDirectories(DirNames* result) {
  result->clear();
  pthread_mutex_lock(&mutex_);
  if (managed_directory_)
    result->push_back(managed_directory_->name());
  pthread_mutex_unlock(&mutex_);
}

ScopedDirLookup::ScopedDirLookup(DirectoryManager* dirman,
                                 const PathString& name) : dirman_(dirman) {
  dir_ = dirman->managed_directory_ &&
         (ComparePathNames(name, dirman->managed_directory_->name()) == 0) ?
         dirman->managed_directory_ : NULL;
  good_ = dir_;
  good_checked_ = false;
}

ScopedDirLookup::~ScopedDirLookup() { }

Directory* ScopedDirLookup::operator -> () const {
  CHECK(good_checked_);
  DCHECK(good_);
  return dir_;
}

ScopedDirLookup::operator Directory* () const {
  CHECK(good_checked_);
  DCHECK(good_);
  return dir_;
}

}  // namespace syncable
