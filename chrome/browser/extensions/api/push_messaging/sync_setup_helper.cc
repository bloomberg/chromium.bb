// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/sync_setup_helper.h"

#include <vector>

#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

SyncSetupHelper::SyncSetupHelper() {}

SyncSetupHelper::~SyncSetupHelper() {}

// Initialize sync clients and profiles for the case where we already
// have some valid sync data we wish to keep.
bool SyncSetupHelper::InitializeSync(Profile* profile) {
  profile_ = profile;
  client_.reset(new ProfileSyncServiceHarness(profile_, username_, password_));

  // Hook up the sync listener.
  if (!client_->InitializeSync()) {
    LOG(ERROR) << "InitializeSync() failed.";
    return false;
  }

  // If the data is not already synced, setup sync.
  if (!client_->IsDataSynced()) {
    client_->SetupSync();
  }

  DVLOG(1) << "InitializeSync awaiting quiescence";

  // Because clients may modify sync data as part of startup (for example local
  // session-releated data is rewritten), we need to ensure all startup-based
  // changes have propagated between the clients.
  // This could take several seconds.
  AwaitQuiescence();

  DVLOG(1) << "InitializeSync reached quiescence";

  return true;
}

// Read the sync signin credentials from a file on the machine.
bool SyncSetupHelper::ReadPasswordFile(const base::FilePath& password_file) {
  std::string file_contents;
  bool success = file_util::ReadFileToString(password_file, &file_contents);
  EXPECT_TRUE(success)
      << "Password file \""
      << password_file.value() << "\" does not exist.";
  if (!success)
    return false;

  std::vector<std::string> tokens;
  std::string delimiters = "\r\n";
  Tokenize(file_contents, delimiters, &tokens);
  EXPECT_EQ(5U, tokens.size()) << "Password file \""
      << password_file.value()
      << "\" must contain exactly five lines of text.";
  if (5U != tokens.size())
    return false;
  username_ = tokens[0];
  password_ = tokens[1];
  client_id_ = tokens[2];
  client_secret_ = tokens[3];
  refresh_token_ = tokens[4];
  return true;
}

bool SyncSetupHelper::AwaitQuiescence() {
  std::vector<ProfileSyncServiceHarness*> clients;
  clients.push_back(client_.get());
  return ProfileSyncServiceHarness::AwaitQuiescence(clients);
}

}  // namespace extensions
