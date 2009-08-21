// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_OBSERVER_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_OBSERVER_H_

class URLRequest;

class BlacklistObserver {
 public:
  // Called when non-visual content is blocked by the privacy blacklist.
  static void ContentBlocked(const URLRequest* request);
};

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_OBSERVER_H_
