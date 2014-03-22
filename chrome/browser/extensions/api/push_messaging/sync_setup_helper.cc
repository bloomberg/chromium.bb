// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/sync_setup_helper.h"

#include <vector>

#include "base/file_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

SyncSetupHelper::SyncSetupHelper() {}

SyncSetupHelper::~SyncSetupHelper() {}

bool SyncSetupHelper::InitializeSync(Profile* profile) {
  profile_ = profile;
  client_.reset(
      ProfileSyncServiceHarness::Create(profile_, username_, password_));

  if (client_->service()->IsSyncEnabledAndLoggedIn())
    return true;

  if (!client_->SetupSync())
    return false;

  // Because clients may modify sync data as part of startup (for example local
  // session-releated data is rewritten), we need to ensure all startup-based
  // changes have propagated between the clients.
  // This could take several seconds.
  AwaitQuiescence();
  return true;
}

// Read the sync signin credentials from a file on the machine.
bool SyncSetupHelper::ReadPasswordFile(const base::FilePath& password_file) {
  // TODO(dcheng): Convert format of config file to JSON.
  std::string file_contents;
  bool success = base::ReadFileToString(password_file, &file_contents);
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
