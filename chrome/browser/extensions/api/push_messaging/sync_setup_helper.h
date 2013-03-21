// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_SYNC_SETUP_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_SYNC_SETUP_HELPER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "net/dns/mock_host_resolver.h"

class Profile;
class ProfileSyncServiceHarness;

namespace extensions {

class SyncSetupHelper {
 public:
  SyncSetupHelper();

  ~SyncSetupHelper();

  // Performs one-time initialization to enable sync for a profile. Does nothing
  // if sync is already enabled for the profile.
  bool InitializeSync(Profile* profile);

  // Helper method used to read GAIA credentials from a local password file.
  // Note: The password file must be a plain text file with two lines.
  // The username is on the first line and the password is on the second line.
  bool ReadPasswordFile(const base::FilePath& passwordFile);

  const std::string& client_id() const { return client_id_; }
  const std::string& client_secret() const { return client_secret_; }
  const std::string& refresh_token() const { return refresh_token_; }

 private:
  // Block until all sync clients have completed their mutual sync cycles.
  // Return true if a quiescent state was successfully reached.
  bool AwaitQuiescence();

  // GAIA account used by the test case.
  std::string username_;

  // GAIA password used by the test case.
  std::string password_;

  // GAIA client id for making the API call to push messaging.
  std::string client_id_;

  // GAIA client secret for making the API call to push messaging.
  std::string client_secret_;

  // GAIA refresh token for making the API call to push messaging.
  std::string refresh_token_;

  // The sync profile used by a test. The profile is owned by the
  // ProfileManager.
  Profile* profile_;

  // Sync client used by a test. A sync client is associated with
  // a sync profile, and implements methods that sync the contents of the
  // profile with the server.
  scoped_ptr<ProfileSyncServiceHarness> client_;

  // This test needs to make live DNS requests for access to
  // GAIA and sync server URLs under google.com. We use a scoped version
  // to override the default resolver while the test is active.
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupHelper);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_SYNC_SETUP_HELPER_H_
