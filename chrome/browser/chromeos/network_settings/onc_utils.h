// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_UTILS_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace onc {

// Parses |json| according to the JSON format. If |json| is a JSON formatted
// dictionary, the function returns the dictionary as a DictionaryValue and
// doesn't modify |error|. Otherwise returns NULL and sets |error| to a
// translated error message.
scoped_ptr<base::DictionaryValue> ReadDictionaryFromJson(
    const std::string& json,
    std::string* error);

// Decrypt the given EncryptedConfiguration |onc| (see the ONC specification)
// using |passphrase|. The resulting UnencryptedConfiguration is returned and
// |error| is not modified. If an error occurs, returns NULL and if additionally
// |error| is not NULL, |error| is set to a user readable error message.
scoped_ptr<base::DictionaryValue> Decrypt(const std::string& passphrase,
                                          const base::DictionaryValue& onc,
                                          std::string* error);

}  // chromeos
}  // onc

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_UTILS_H_
