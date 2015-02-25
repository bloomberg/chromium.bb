// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_

namespace safe_browsing {

// Initiates a download of the SRT. On either success or failure, the
// appropriate SRT notification bubble will be displayed to the user.
void FetchSRTAndDisplayBubble();

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_
