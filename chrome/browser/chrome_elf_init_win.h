// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_ELF_INIT_WIN_H_
#define CHROME_BROWSER_CHROME_ELF_INIT_WIN_H_

#include <vector>

#include "base/strings/string16.h"

// Field trial name and full name for the blacklist disabled group.
extern const char kBrowserBlacklistTrialName[];
extern const char kBrowserBlacklistTrialDisabledGroupName[];

// Prepare any initialization code for Chrome Elf's setup (This will generally
// only affect future runs since Chrome Elf is already setup by this point).
void InitializeChromeElf();

// Add the blacklist from the finch configs in the registry.
void AddFinchBlacklistToRegistry();

// Set the required state for an enabled browser blacklist.
void BrowserBlacklistBeaconSetup();

// Retrieves the set of blacklisted modules that are loaded in the process.
// Returns true if successful, false otherwise.
bool GetLoadedBlacklistedModules(std::vector<base::string16>* module_names);

#endif  // CHROME_BROWSER_CHROME_ELF_INIT_WIN_H_
