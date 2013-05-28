// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_DETAILS_H_
#define CHROME_BROWSER_HISTORY_HISTORY_DETAILS_H_

namespace history {

// Base class for history notifications. This needs only a virtual destructor
// so that the history service's broadcaster can delete it when the request
// is complete.
struct HistoryDetails {
 public:
  virtual ~HistoryDetails() {}
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_DETAILS_H_
