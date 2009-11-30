// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_UI_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_UI_H_

class URLRequest;

// Set of routines to display blacklist-related UI.
class BlacklistUI {
 public:
  // Called on IO thread when non-visual content is blocked by a privacy
  // blacklist.
  static void OnNonvisualContentBlocked(const URLRequest* request);
};

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_UI_H_
