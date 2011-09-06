// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_
#pragma once

#include <vector>

#include "chrome/browser/history/history_types.h"

namespace base {
class Time;
}

namespace typed_urls_helper {

// Gets the typed URLs from a specific sync profile.
std::vector<history::URLRow> GetTypedUrlsFromClient(int index);

// Adds a URL to the history DB for a specific sync profile (just registers a
// new visit if the URL already exists).
void AddUrlToHistory(int index, const GURL& url);

// Deletes a URL from the history DB for a specific sync profile.
void DeleteUrlFromHistory(int index, const GURL& url);

// Returns true if all clients match the verifier profile.
void AssertAllProfilesHaveSameURLsAsVerifier();

// Checks that the two vectors contain the same set of URLRows (possibly in
// a different order).
void AssertURLRowVectorsAreEqual(
    const std::vector<history::URLRow>& left,
    const std::vector<history::URLRow>& right);

// Checks that the passed URLRows are equivalent.
void AssertURLRowsAreEqual(const history::URLRow& left,
                           const history::URLRow& right);

// Returns a unique timestamp to use when generating page visits
// (HistoryService does not like having identical timestamps and will modify
// the timestamps behind the scenes if it encounters them, which leads to
// spurious test failures when the resulting timestamps aren't what we
// expect).
base::Time GetTimestamp();

}  // namespace typed_urls_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_
