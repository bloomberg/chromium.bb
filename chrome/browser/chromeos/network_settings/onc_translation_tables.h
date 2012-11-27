// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_TRANSLATION_TABLES_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_TRANSLATION_TABLES_H_

namespace chromeos {
namespace onc {

struct StringTranslationEntry {
  const char* onc_value;
  const char* shill_value;
};

// These tables contain the mapping from ONC strings to Shill strings.
// These are NULL-terminated arrays.
extern const StringTranslationEntry kNetworkTypeTable[];
extern const StringTranslationEntry kVPNTypeTable[];

}  // namespace onc
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_TRANSLATION_TABLES_H_
