// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/locally_managed_user_constants.h"

#include "chromeos/cryptohome/cryptohome_parameters.h"

namespace chromeos {

const char kManagedUserTokenFilename[] = "token";

const char kCryptohomeManagedUserKeyLabel[] = "managed";
const char kCryptohomeMasterKeyLabel[] = "master";
const char kLegacyCryptohomeManagedUserKeyLabel[] = "default-0";
const char kLegacyCryptohomeMasterKeyLabel[] = "default-1";

const int kCryptohomeManagedUserKeyPrivileges =
    cryptohome::PRIV_AUTHORIZED_UPDATE | cryptohome::PRIV_MOUNT;
const int kCryptohomeManagedUserIncompleteKeyPrivileges =
    cryptohome::PRIV_MIGRATE | cryptohome::PRIV_MOUNT;

}  // namespace chromeos
