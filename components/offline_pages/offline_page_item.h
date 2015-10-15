// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_ITEM_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_ITEM_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace offline_pages {

// Metadata of the offline page.
struct OfflinePageItem {
 public:
  // Note that this should match with Flags enum in offline_pages.proto.
  enum Flags {
    NO_FLAG = 0,
    MARKED_FOR_DELETION = 0x1,
  };

  OfflinePageItem();
  OfflinePageItem(const GURL& url,
                  int64 bookmark_id,
                  const base::FilePath& file_path,
                  int64 file_size);
  OfflinePageItem(const GURL& url,
                  int64 bookmark_id,
                  const base::FilePath& file_path,
                  int64 file_size,
                  const base::Time& creation_time);
  ~OfflinePageItem();

  // Gets a URL of the file under |file_path|.
  GURL GetOfflineURL() const;

  // Returns true if the page has been marked for deletion. This allows an undo
  // in a short time period. After that, the marked page will be deleted.
  bool IsMarkedForDeletion() const;

  // Sets/clears the mark for deletion.
  void MarkForDeletion();
  void ClearMarkForDeletion();

  // The URL of the page.
  GURL url;
  // The Bookmark ID related to the offline page.
  int64 bookmark_id;
  // Version of the offline page item.
  int version;
  // The file path to the archive with a local copy of the page.
  base::FilePath file_path;
  // The size of the offline copy.
  int64 file_size;
  // The time when the offline archive was created.
  base::Time creation_time;
  // The time when the offline archive was last accessed.
  base::Time last_access_time;
  // Number of times that the offline archive has been accessed.
  int access_count;
  // Flags about the state and behavior of the offline page.
  Flags flags;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_ITEM_H_
