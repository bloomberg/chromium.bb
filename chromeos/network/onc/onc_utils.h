// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_UTILS_H_
#define CHROMEOS_NETWORK_ONC_ONC_UTILS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace onc {

// A valid but empty (no networks and no certificates) and unencrypted
// configuration.
CHROMEOS_EXPORT extern const char kEmptyUnencryptedConfiguration[];

// Parses |json| according to the JSON format. If |json| is a JSON formatted
// dictionary, the function returns the dictionary as a DictionaryValue.
// Otherwise returns NULL.
CHROMEOS_EXPORT scoped_ptr<base::DictionaryValue> ReadDictionaryFromJson(
    const std::string& json);

// Decrypt the given EncryptedConfiguration |onc| (see the ONC specification)
// using |passphrase|. The resulting UnencryptedConfiguration is returned. If an
// error occurs, returns NULL.
CHROMEOS_EXPORT scoped_ptr<base::DictionaryValue> Decrypt(
    const std::string& passphrase,
    const base::DictionaryValue& onc);

// For logging only: strings not user facing.
CHROMEOS_EXPORT std::string GetSourceAsString(ONCSource source);

}  // chromeos
}  // onc

#endif  // CHROMEOS_NETWORK_ONC_ONC_UTILS_H_
