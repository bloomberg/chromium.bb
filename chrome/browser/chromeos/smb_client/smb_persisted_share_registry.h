// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_PERSISTED_SHARE_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_PERSISTED_SHARE_REGISTRY_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/chromeos/smb_client/smb_share_info.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {
namespace smb_client {

class SmbUrl;

// Handles saving of SMB shares in the user's Profile.
class SmbPersistedShareRegistry {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit SmbPersistedShareRegistry(Profile* profile);

  // Disallow copy/assign.
  SmbPersistedShareRegistry() = delete;
  SmbPersistedShareRegistry(const SmbPersistedShareRegistry&) = delete;
  SmbPersistedShareRegistry& operator=(const SmbPersistedShareRegistry&) =
      delete;

  // Save |share| in the user's profile. If a saved share already exists with
  // the url share.share_url(), that saved share will be overwritten.
  void Save(const SmbShareInfo& share);

  // Delete the saved share with the URL |share_url|.
  void Delete(const SmbUrl& share_url);

  // Return the saved share with URL |share_url|, or the empty Optional<> if no
  // share is found.
  base::Optional<SmbShareInfo> Get(const SmbUrl& share_url) const;

  // Return a list of all saved shares.
  std::vector<SmbShareInfo> GetAll() const;

 private:
  Profile* const profile_;
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_PERSISTED_SHARE_REGISTRY_H_
