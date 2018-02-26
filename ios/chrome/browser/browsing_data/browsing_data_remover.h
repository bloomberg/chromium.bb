// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"

// BrowsingDataRemover is responsible for removing data related to
// browsing: history, downloads, cookies, ...
class BrowsingDataRemover : public KeyedService {
 public:
  BrowsingDataRemover() = default;
  ~BrowsingDataRemover() override = default;

  // Is the service currently in the process of removing data?
  virtual bool IsRemoving() const = 0;

  // Removes browsing data for the given |time_period| with data types specified
  // by |remove_mask|. The |callback| is invoked asynchronously when the data
  // has been removed.
  virtual void Remove(browsing_data::TimePeriod time_period,
                      BrowsingDataRemoveMask remove_mask,
                      base::OnceClosure callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemover);
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
