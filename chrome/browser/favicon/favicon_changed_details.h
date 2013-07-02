// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_CHANGED_DETAILS_H_
#define CHROME_BROWSER_FAVICON_FAVICON_CHANGED_DETAILS_H_

#include <set>

#include "chrome/browser/history/history_details.h"
#include "url/gurl.h"

// Details for chrome::NOTIFICATION_FAVICON_CHANGED.
struct FaviconChangedDetails : public history::HistoryDetails {
  FaviconChangedDetails();
  virtual ~FaviconChangedDetails();

  std::set<GURL> urls;
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_CHANGED_DETAILS_H_
