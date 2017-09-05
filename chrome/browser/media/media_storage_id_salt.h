// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STORAGE_ID_SALT_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STORAGE_ID_SALT_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"

class PrefRegistrySimple;
class PrefService;

// MediaStorageIDSalt is responsible for creating and retrieving a salt string
// that is used when creating Storage IDs.
class MediaStorageIdSalt {
 public:
  enum { kSaltLength = 32 };

  // Retrieves the current salt. If one does not currently exist it is created.
  static std::vector<uint8_t> GetSalt(PrefService* pref_service);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MediaStorageIdSalt);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STORAGE_ID_SALT_H_
