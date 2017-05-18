// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_IMPL_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_handler.h"

class GURL;
namespace bookmarks {
class BookmarkModel;
}


namespace history {
class HistoryService;
}

namespace favicon {

class FaviconService;
struct FaviconURL;

// FaviconDriverImpl is a partial implementation of FaviconDriver that allow
// sharing implementation between different embedder.
//
// FaviconDriverImpl works with FaviconHandlers to fetch the favicons. It
// fetches the given page's icons, requesting them from history backend. If the
// icon is not available or expired, the icon will be downloaded and saved in
// the history backend.
class FaviconDriverImpl : public FaviconDriver,
                          public FaviconHandler::Delegate {
 public:
  // FaviconDriver implementation.
  void FetchFavicon(const GURL& url) override;

  // FaviconHandler::Delegate implementation.
  bool IsBookmarked(const GURL& url) override;

  // Returns whether the driver is waiting for a download to complete or for
  // data from the FaviconService. Reserved for testing.
  bool HasPendingTasksForTest();

 protected:
  FaviconDriverImpl(FaviconService* favicon_service,
                    history::HistoryService* history_service,
                    bookmarks::BookmarkModel* bookmark_model);
  ~FaviconDriverImpl() override;

  // Informs FaviconService that the favicon for |url| is out of date. If
  // |force_reload| is true, then discard information about favicon download
  // failures.
  void SetFaviconOutOfDateForPage(const GURL& url, bool force_reload);

  // Broadcasts new favicon URL candidates to FaviconHandlers.
  void OnUpdateCandidates(const GURL& page_url,
                          const std::vector<FaviconURL>& candidates,
                          const GURL& manifest_url);

 protected:
  history::HistoryService* history_service() { return history_service_; }

  FaviconService* favicon_service() { return favicon_service_; }

 private:
  // KeyedServices used by FaviconDriverImpl. They may be null during testing,
  // but if they are defined, they must outlive the FaviconDriverImpl.
  FaviconService* favicon_service_;
  history::HistoryService* history_service_;
  bookmarks::BookmarkModel* bookmark_model_;

  // FaviconHandlers used to download the different kind of favicons.
  std::vector<std::unique_ptr<FaviconHandler>> handlers_;

  DISALLOW_COPY_AND_ASSIGN(FaviconDriverImpl);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_IMPL_H_
