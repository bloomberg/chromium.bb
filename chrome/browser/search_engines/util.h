// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_UTIL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_UTIL_H_

// This file contains utility functions for search engine functionality.

#include "base/string16.h"

class Profile;

// Returns the short name of the default search engine, or the empty string if
// none is set. |profile| may be NULL.
string16 GetDefaultSearchEngineName(Profile* profile);

#endif  // CHROME_BROWSER_SEARCH_ENGINES_UTIL_H_
