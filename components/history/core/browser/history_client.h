// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/keyed_service/core/keyed_service.h"
#include "sql/init_status.h"
#include "url/gurl.h"

namespace history {

struct URLAndTitle {
  GURL url;
  base::string16 title;
};

// This class abstracts operations that depend on the embedder's environment,
// e.g. Chrome.
class HistoryClient : public KeyedService {
 public:
  HistoryClient();

  // Waits until the bookmarks have been loaded.
  //
  // Must not be called from the main thread.
  virtual void BlockUntilBookmarksLoaded();

  // Returns true if the specified URL is bookmarked.
  //
  // If not on the main thread, then BlockUntilBookmarksLoaded must be called.
  virtual bool IsBookmarked(const GURL& url);

  // Returns, by reference in |bookmarks|, the set of bookmarked urls and their
  // titles. This returns the unique set of URLs. For example, if two bookmarks
  // reference the same URL only one entry is added even if the title are not
  // the same.
  //
  // If not on the main thread, then BlockUntilBookmarksLoaded must be called.
  virtual void GetBookmarks(std::vector<URLAndTitle>* bookmarks);

  // Notifies the embedder that there was a problem reading the database.
  //
  // Must be called from the main thread.
  virtual void NotifyProfileError(sql::InitStatus init_status);

  // Returns whether database errors should be reported to the crash server.
  virtual bool ShouldReportDatabaseError();

 protected:
  DISALLOW_COPY_AND_ASSIGN(HistoryClient);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_
