// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/token_migrator.h"

#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/util/user_settings.h"
#include "chrome/common/net/gaia/gaia_constants.h"

const FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

const FilePath::CharType kBookmarkSyncUserSettingsDatabase[] =
    FILE_PATH_LITERAL("BookmarkSyncSettings.sqlite3");

using browser_sync::UserSettings;

TokenMigrator::TokenMigrator(ProfileSyncService* service,
                             const FilePath& profile_path)
      : service_(service),
        database_location_(profile_path.Append(kSyncDataFolderName)) {
}

TokenMigrator::~TokenMigrator() {
}

void TokenMigrator::TryMigration() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      NewRunnableMethod(this, &TokenMigrator::LoadTokens));
}

void TokenMigrator::LoadTokens() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  scoped_ptr<UserSettings> user_settings(new UserSettings());
  FilePath settings_db_file =
      database_location_.Append(FilePath(kBookmarkSyncUserSettingsDatabase));
  if (!user_settings->Init(settings_db_file))
    return;

  if (!user_settings->GetLastUserAndServiceToken(GaiaConstants::kSyncService,
                                                 &username_, &token_))
    return;

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &TokenMigrator::PostTokensBack));
}

void TokenMigrator::PostTokensBack() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  service_->LoadMigratedCredentials(username_, token_);
}
