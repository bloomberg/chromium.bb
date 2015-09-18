// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_
#define CHROME_BROWSER_PREFS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "components/syncable_prefs/pref_model_associator_client.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class ChromePrefModelAssociatorClient
    : public syncable_prefs::PrefModelAssociatorClient {
 public:
  // Returns the global instance.
  static ChromePrefModelAssociatorClient* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ChromePrefModelAssociatorClient>;

  ChromePrefModelAssociatorClient();
  ~ChromePrefModelAssociatorClient() override;

  // syncable_prefs::PrefModelAssociatorClient implementation.
  bool IsMergeableListPreference(const std::string& pref_name) const override;
  bool IsMergeableDictionaryPreference(
      const std::string& pref_name) const override;
  bool IsMigratedPreference(const std::string& new_pref_name,
                            std::string* old_pref_name) const override;
  bool IsOldMigratedPreference(const std::string& old_pref_name,
                               std::string* new_pref_name) const override;

  DISALLOW_COPY_AND_ASSIGN(ChromePrefModelAssociatorClient);
};

#endif  // CHROME_BROWSER_PREFS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_
