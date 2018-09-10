// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/os_crypt_features_mac.h"

namespace os_crypt {
namespace features {

// Prevents overwriting the encryption key in the Keychain on Mac if the key is
// unavailable, but there is a preference set that the key was created in the
// past.
const base::Feature kPreventEncryptionKeyOverwrites = {
    "PreventEncryptionKeyOverwrites", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace os_crypt
