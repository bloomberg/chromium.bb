// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/offline_pages/core/client_id.h"
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
                  int64_t offline_id,
                  const ClientId& client_id,
                  const base::FilePath& file_path,
                  int64_t file_size);
  OfflinePageItem(const GURL& url,
                  int64_t offline_id,
                  const ClientId& client_id,
                  const base::FilePath& file_path,
                  int64_t file_size,
                  const base::Time& creation_time);
  OfflinePageItem(const GURL& url,
                  int64_t offline_id,
                  const ClientId& client_id,
                  const base::FilePath& file_path,
                  int64_t file_size,
                  const base::Time& creation_time,
                  const std::string& origin_package);
  OfflinePageItem(const OfflinePageItem& other);
  ~OfflinePageItem();

  bool operator==(const OfflinePageItem& other) const;

  // The URL of the page. This is the last committed URL. In the case that
  // redirects occur, access |original_url| for the original URL.
  GURL url;
  // The primary key/ID for this page in offline pages internal database.
  int64_t offline_id;

  // The Client ID (external) related to the offline page. This is opaque
  // to our system, but useful for users of offline pages who want to map
  // their ids to our saved pages.
  ClientId client_id;

  // The file path to the archive with a local copy of the page.
  base::FilePath file_path;
  // The size of the offline copy.
  int64_t file_size;
  // The time when the offline archive was created.
  base::Time creation_time;
  // The time when the offline archive was last accessed.
  base::Time last_access_time;
  // Number of times that the offline archive has been accessed.
  int access_count;
  // The title of the page at the time it was saved.
  base::string16 title;
  // Flags about the state and behavior of the offline page.
  Flags flags;
  // The original URL of the page if not empty. Otherwise, this is set to empty
  // and |url| should be accessed instead.
  GURL original_url;
  // The app, if any, that the item was saved on behalf of.
  // Empty string implies Chrome.
  std::string request_origin;
  // System download id.
  int64_t system_download_id;
  // The most recent time when the file was discovered missing.
  // NULL time implies the file is not missing.
  base::Time file_missing_time;
  // Number of attemps for upgrading the MHTML page into new format.
  int upgrade_attempt;
  // Digest of the page calculated when page is saved, in order to tell if the
  // page can be trusted. This field will always be an empty string for
  // temporary and shared pages.
  std::string digest;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_H_
