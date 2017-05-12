// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_FETCHER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_FETCHER_WIN_H_

#include "base/callback.h"
#include "base/files/file_path.h"

namespace safe_browsing {

// Type of callback that is called when the network request to fetch the Chrome
// Cleaner binary has been completed. The callback will be passed the filepath
// and http response code as returned by net::URLFetcher.
using ChromeCleanerFetchedCallback =
    base::OnceCallback<void(base::FilePath, int /*http response code*/)>;

void FetchChromeCleaner(ChromeCleanerFetchedCallback fetched_callback);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_FETCHER_WIN_H_
