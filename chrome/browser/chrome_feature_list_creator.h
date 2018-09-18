// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_FEATURE_LIST_CREATOR_H_
#define CHROME_BROWSER_CHROME_FEATURE_LIST_CREATOR_H_

#include "base/macros.h"
#include "components/prefs/pref_service.h"

// Responsible for creating feature list and all its necessary parameters.
// This class is currently WIP and doesn't do what it's meant to do.
// TODO(hanxi): Finish implementation, https://crbug.com/848615.
class ChromeFeatureListCreator {
 public:
  ChromeFeatureListCreator();
  ~ChromeFeatureListCreator();

  // Gets the pref store that is used to create feature list.
  scoped_refptr<PersistentPrefStore> GetPrefStore();

  // Passing ownership of the |simple_local_state_| to the caller.
  std::unique_ptr<PrefService> TakePrefService();

  // Initializes all necessary parameters to create the feature list and calls
  // base::FeatureList::SetInstance() to set the global instance.
  void CreateFeatureList();

  void CreatePrefServiceForTesting();

 private:
  void CreatePrefService();

  scoped_refptr<PersistentPrefStore> pref_store_;

  // If TakePrefService() is called, the caller will take the ownership
  // of this variable. Stop using this variable afterwards.
  std::unique_ptr<PrefService> simple_local_state_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFeatureListCreator);
};

#endif  // CHROME_BROWSER_CHROME_FEATURE_LIST_CREATOR_H_
