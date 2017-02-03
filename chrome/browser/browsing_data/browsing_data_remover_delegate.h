// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_DELEGATE_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_DELEGATE_H_

namespace content {
class BrowsingDataFilterBuilder;
}

class BrowsingDataRemoverDelegate {
 public:
  virtual ~BrowsingDataRemoverDelegate() {}

  virtual void RemoveEmbedderData(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      const content::BrowsingDataFilterBuilder& filter_builder,
      int origin_type_mask,
      const base::Closure& callback) = 0;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_DELEGATE_H_
