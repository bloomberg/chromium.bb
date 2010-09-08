// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_path.h"
#include "base/task.h"

class ProfileSyncService;

// The TokenMigrator provides a bridge between the IO thread and
// the UI thread to load the old-style credentials from the user
// settings DB, and post them back to the UI thread to store the
// token in the token service.
class TokenMigrator {
 public:
  TokenMigrator(ProfileSyncService* service,
                const FilePath& profile_path);
  ~TokenMigrator();

  // This is called on the UI thread, it only posts a task.
  // If migration succeeds, the tokens will become available in
  // the token service.
  void TryMigration();

 private:
  // This runs as a deferred task on the DB thread.
  void LoadTokens();

  // This runs as a deferred task on the UI thread (only after the DB thread is
  // finished with the data.
  void PostTokensBack();

  // The username and token retrieved from the user settings DB.
  std::string username_;
  std::string token_;

  // The ProfileSyncService owns this object, so this pointer is valid when
  // PostTokensBack is called.
  ProfileSyncService* service_;

  // Pending tasks, stored so they can be canceled if this object is destroyed.
  CancelableTask* loading_task_;

  // The directory to search for the user settings database.
  FilePath database_location_;

  DISALLOW_COPY_AND_ASSIGN(TokenMigrator);
};

// We ensure this object will outlive its tasks, so don't need refcounting.
DISABLE_RUNNABLE_METHOD_REFCOUNT(TokenMigrator);
