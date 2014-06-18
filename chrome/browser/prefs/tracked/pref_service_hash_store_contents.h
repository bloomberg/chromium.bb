// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_TRACKED_PREF_SERVICE_HASH_STORE_CONTENTS_H_
#define CHROME_BROWSER_PREFS_TRACKED_PREF_SERVICE_HASH_STORE_CONTENTS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/prefs/tracked/hash_store_contents.h"

class PrefRegistrySimple;
class PrefService;

// Implements HashStoreContents by storing hashes in a PrefService. Multiple
// separate hash stores may coexist in the PrefService by using distinct hash
// store IDs.
// TODO(erikwright): This class is only used to recreate preference state as in
// M35, to test migration behaviour. Remove this class when
// ProfilePrefStoreManagerTest no longer depends on it.
class PrefServiceHashStoreContents : public HashStoreContents {
 public:
  // Constructs a HashStoreContents that stores hashes in |pref_service|.
  // Multiple hash stores can use the same |pref_service| with distinct
  // |hash_store_id|s.
  //
  // |pref_service| must have previously been configured using |RegisterPrefs|.
  PrefServiceHashStoreContents(const std::string& hash_store_id,
                               PrefService* pref_service);

  // A dictionary pref which maps profile names to dictionary values which hold
  // hashes of profile prefs that we track to detect changes that happen outside
  // of Chrome.
  static const char kProfilePreferenceHashes[];

  // The name of a dict that is stored as a child of
  // |prefs::kProfilePreferenceHashes|. Each child node is a string whose name
  // is a hash store ID and whose value is the super MAC for the corresponding
  // hash store.
  static const char kHashOfHashesDict[];

  // The name of a dict that is stored as a child of
  // |prefs::kProfilePreferenceHashes|. Each child node is a number whose name
  // is a hash store ID and whose value is the version of the corresponding
  // hash store.
  static const char kStoreVersionsDict[];

  // Registers required preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Deletes stored hashes for all profiles from |pref_service|.
  static void ResetAllPrefHashStores(PrefService* pref_service);

  // HashStoreContents implementation
  virtual std::string hash_store_id() const OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual bool IsInitialized() const OVERRIDE;
  virtual const base::DictionaryValue* GetContents() const OVERRIDE;
  virtual scoped_ptr<MutableDictionary> GetMutableContents() OVERRIDE;
  virtual std::string GetSuperMac() const OVERRIDE;
  virtual void SetSuperMac(const std::string& super_mac) OVERRIDE;

 private:
  const std::string hash_store_id_;
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceHashStoreContents);
};

#endif  // CHROME_BROWSER_PREFS_TRACKED_PREF_SERVICE_HASH_STORE_CONTENTS_H_
