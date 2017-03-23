// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_DELEGATE_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_DELEGATE_H_

#include "base/callback_forward.h"

class GURL;

namespace base {
class Time;
}

namespace content {
class BrowsingDataFilterBuilder;
}

namespace storage {
class SpecialStoragePolicy;
}

class BrowsingDataRemoverDelegate {
 public:
  virtual ~BrowsingDataRemoverDelegate() {}

  // Determines whether |origin| matches |origin_type_mask|
  // given the |special_storage_policy|. |origin_type_mask| should only contain
  // embedder-specific datatypes.
  virtual bool DoesOriginMatchEmbedderMask(
      int origin_type_mask,
      const GURL& origin,
      storage::SpecialStoragePolicy* special_storage_policy) const = 0;

  // Removes embedder-specific data.
  virtual void RemoveEmbedderData(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      const content::BrowsingDataFilterBuilder& filter_builder,
      int origin_type_mask,
      const base::Closure& callback) = 0;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_DELEGATE_H_
