// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__

#include "base/basictypes.h"
#include "chrome/browser/sync/profile_sync_factory.h"

class CommandLine;
class Profile;

class ProfileSyncFactoryImpl : public ProfileSyncFactory {
 public:
  ProfileSyncFactoryImpl(Profile* profile, CommandLine* command_line);
  virtual ~ProfileSyncFactoryImpl() {}

  // ProfileSyncFactory interface.
  virtual ProfileSyncService* CreateProfileSyncService();

  virtual SyncComponents CreateBookmarkSyncComponents(
      ProfileSyncService* profile_sync_service);

  virtual SyncComponents CreatePreferenceSyncComponents(
      ProfileSyncService* profile_sync_service);

 private:
  Profile* profile_;
  CommandLine* command_line_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncFactoryImpl);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__
