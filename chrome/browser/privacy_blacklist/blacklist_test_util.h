// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_TEST_UTIL_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_TEST_UTIL_H_

#include <vector>

#include "base/scoped_temp_dir.h"
#include "chrome/browser/privacy_blacklist/blacklist_manager.h"
#include "chrome/test/testing_profile.h"

class FilePath;

// Testing profile which uses a temporary directory.
class BlacklistTestingProfile : public TestingProfile {
 public:
  BlacklistTestingProfile();

  BlacklistManager* GetBlacklistManager() { return blacklist_manager_; }
  void set_blacklist_manager(BlacklistManager* manager) {
    blacklist_manager_ = manager;
  }

 private:
  ScopedTempDir temp_dir_;

  BlacklistManager* blacklist_manager_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistTestingProfile);
};

// Path provider which allows easy updates from the outside and sends
// notifications for each change.
class TestBlacklistPathProvider : public BlacklistPathProvider {
 public:
  explicit TestBlacklistPathProvider(Profile* profile);

  // BlacklistPathProvider:
  virtual bool AreBlacklistPathsReady() const;
  virtual std::vector<FilePath> GetPersistentBlacklistPaths();
  virtual std::vector<FilePath> GetTransientBlacklistPaths();

  // Adds a path to the provder and sends an update notification.
  void AddPersistentPath(const FilePath& path);
  void AddTransientPath(const FilePath& path);

  // Removes all paths from the provider and send an update notification.
  void clear();

 private:
  void SendUpdateNotification();

  Profile* profile_;

  // Keep track of added paths.
  std::vector<FilePath> persistent_paths_;
  std::vector<FilePath> transient_paths_;

  DISALLOW_COPY_AND_ASSIGN(TestBlacklistPathProvider);
};

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_TEST_UTIL_H_
