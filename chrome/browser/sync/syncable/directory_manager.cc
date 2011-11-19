// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/directory_manager.h"

#include <map>
#include <set>
#include <iterator>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/port.h"
#include "base/string_util.h"
#include "chrome/browser/sync/syncable/syncable.h"

using browser_sync::Cryptographer;

namespace syncable {

static const FilePath::CharType kSyncDataDatabaseFilename[] =
    FILE_PATH_LITERAL("SyncData.sqlite3");

// static
const FilePath DirectoryManager::GetSyncDataDatabaseFilename() {
  return FilePath(kSyncDataDatabaseFilename);
}

const FilePath DirectoryManager::GetSyncDataDatabasePath() const {
  return root_path_.Append(GetSyncDataDatabaseFilename());
}

DirectoryManager::DirectoryManager(const FilePath& path)
    : root_path_(path),
      managed_directory_(NULL),
      cryptographer_(new Cryptographer) {
}

DirectoryManager::~DirectoryManager() {
  base::AutoLock lock(lock_);
  DCHECK_EQ(managed_directory_, static_cast<Directory*>(NULL))
      << "Dir " << managed_directory_->name() << " not closed!";
}

bool DirectoryManager::Open(
    const std::string& name,
    DirectoryChangeDelegate* delegate,
    const browser_sync::WeakHandle<TransactionObserver>&
        transaction_observer) {
  bool was_open = false;
  const DirOpenResult result =
      OpenImpl(name, GetSyncDataDatabasePath(), delegate,
               transaction_observer, &was_open);
  return syncable::OPENED == result;
}

// Opens a directory.  Returns false on error.
DirOpenResult DirectoryManager::OpenImpl(
    const std::string& name,
    const FilePath& path,
    DirectoryChangeDelegate* delegate,
    const browser_sync::WeakHandle<TransactionObserver>&
        transaction_observer,
    bool* was_open) {
  bool opened = false;
  {
    base::AutoLock lock(lock_);
    // Check to see if it's already open.
    if (managed_directory_) {
      DCHECK_EQ(base::strcasecmp(name.c_str(),
                                 managed_directory_->name().c_str()), 0)
          << "Can't open more than one directory.";
      opened = *was_open = true;
    }
  }

  if (opened)
    return syncable::OPENED;
  // Otherwise, open it.

  scoped_ptr<Directory> dir(new Directory);
  const DirOpenResult result =
      dir->Open(path, name, delegate, transaction_observer);
  if (syncable::OPENED == result) {
    base::AutoLock lock(lock_);
    managed_directory_ = dir.release();
  }
  return result;
}

// Marks a directory as closed.  It might take a while until all the file
// handles and resources are freed by other threads.
void DirectoryManager::Close(const std::string& name) {
  // Erase from mounted and opened directory lists.
  {
    base::AutoLock lock(lock_);
    if (!managed_directory_ ||
        base::strcasecmp(name.c_str(),
                         managed_directory_->name().c_str()) != 0) {
      // It wasn't open.
      return;
    }
  }

  delete managed_directory_;
  managed_directory_ = NULL;
}

void DirectoryManager::FinalSaveChangesForAll() {
  base::AutoLock lock(lock_);
  if (managed_directory_)
    managed_directory_->SaveChanges();
}

void DirectoryManager::GetOpenDirectories(DirNames* result) {
  result->clear();
  base::AutoLock lock(lock_);
  if (managed_directory_)
    result->push_back(managed_directory_->name());
}

ScopedDirLookup::ScopedDirLookup(DirectoryManager* dirman,
                                 const std::string& name) : dirman_(dirman) {
  dir_ = dirman->managed_directory_ &&
         (base::strcasecmp(name.c_str(),
                           dirman->managed_directory_->name().c_str()) == 0) ?
         dirman->managed_directory_ : NULL;
  good_ = dir_ != NULL;
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
