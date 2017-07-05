// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_ITEM_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_ITEM_H_

#include <string>

#include "base/time/time.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "url/gurl.h"

namespace offline_pages {

// Data object representing an item progressing through the prefetching process
// from the moment a URL is requested by a client until its processing is done,
// successfully or not.
// Instances of this class are in-memory representations of items in (or to be
// inserted into) the persistent prefetching data store.
struct PrefetchItem {
  PrefetchItem();
  explicit PrefetchItem(const PrefetchItem& other);

  ~PrefetchItem();

  bool operator==(const PrefetchItem& other) const;
  bool operator!=(const PrefetchItem& other) const;

  // Primary key that stays consistent between prefetch item, request and
  // offline page.
  int64_t offline_id = 0;

  // Primary key/ID for this prefetch item (See |base::GenerateGUID()|). This
  // value will be reused when communicating with other systems accepting GUID
  // identifiers for operations linked to this item.
  std::string guid;

  // Data composed of a namespace and an uid values to allow this item to be
  // uniquely identified by the client that requested it.
  ClientId client_id;

  // Current prefetching progress state.
  PrefetchItemState state = PrefetchItemState::NEW_REQUEST;

  // The URL of the page the client requested to be prefetched.
  GURL url;

  // The final URL whose page was actually included in a successfully created
  // archive after redirects, if it was different than the |url|. It will be
  // left empty if they are the same.
  GURL final_archived_url;

  // Number of times an attempt was made to generate an archive for this item.
  int request_archive_attempt_count = 0;

  // Name used to identify the archiving operation being executed by the
  // prefetching service for processing this item's URL. It is used as the
  // |operation_name| reported by an incoming GCM message and in the
  // |GetOperationRequest.name| field of the respective GetOperation RPC.
  std::string operation_name;

  // The name specific to this item's archive that can be used to build the URL
  // to allow the downloading of that archive. Will only be set when and if an
  // archive was successfully created for this item. It will be kept empty
  // otherwise.
  std::string archive_body_name;

  // The final size of the generated archive that contains this item's page
  // snapshot. The archive might also include other articles in a bundle so the
  // length is not necessarily directly related to this item's page contents.
  // Will only be set when and if an archive was successfully created for this
  // item. It holds a negative value otherwise.
  int64_t archive_body_length = -1;

  // Time when this item was inserted into the store with the URL to be
  // prefetched.
  base::Time creation_time;

  // Time used for the expiration of the item depending on the applicable policy
  // for its current state. It is initially set with the same value as
  // |creation_time|. Its value is "refreshed" to the current time on some state
  // transitions considered significant for the prefetching process.
  base::Time freshness_time;

  // The reason why the item was set to the FINISHED state. Should be
  // disregarded until reaching that state.
  PrefetchItemErrorCode error_code = PrefetchItemErrorCode::SUCCESS;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_ITEM_H_
