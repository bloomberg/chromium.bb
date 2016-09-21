// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_HELPER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_HELPER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "ios/chrome/browser/browsing_data/ios_chrome_browsing_data_remover.h"

namespace ios {
class ChromeBrowserState;
}

// A helper class that serializes execution of IOSChromeBrowsingDataRemover
// methods since the IOSChromeBrowsingDataRemover APIs are not re-entrant.
class BrowsingDataRemoverHelper
    : public IOSChromeBrowsingDataRemover::Observer {
 public:
  BrowsingDataRemoverHelper();
  ~BrowsingDataRemoverHelper() override;

  // Removes the specified browsing data associated with |browser_state|. Calls
  // |callback| when the browsing data is actually removed. |browser_state|
  // cannot be null and must not be off the record.
  // |callback| is called on the main thread.
  // Note: Removal operations are not necessarily processed in the sequence that
  // they are received in.
  void Remove(ios::ChromeBrowserState* browser_state,
              int remove_mask,
              browsing_data::TimePeriod time_period,
              const base::Closure& callback);

 private:
  // Encapsulates the information that is needed to remove browsing data from
  // a ChromeBrowserState.
  struct BrowsingDataRemovalInfo {
    // Creates a BrowsingDataRemovalInfo with a single callback |callback|.
    BrowsingDataRemovalInfo(int remove_mask,
                            browsing_data::TimePeriod time_period,
                            const base::Closure& callback);
    ~BrowsingDataRemovalInfo();
    // The mask of all the types of browsing data that needs to be removed.
    // Obtained from BrowsingDataRemoved::RemoveDataMask.
    int remove_mask;
    // Time period for which the user wants to remove the data.
    browsing_data::TimePeriod time_period;
    // The vector of callbacks that need to be run when browsing data is
    // actually removed.
    std::vector<base::Closure> callbacks;
  };

  // IOSChromeBrowsingDataRemover::Observer methods.
  void OnIOSChromeBrowsingDataRemoverDone() override;

  // Removes the browsing data using IOSChromeBrowsingDataRemover.
  // IOSChromeBrowsingDataRemover
  // must not be running.
  void DoRemove(ios::ChromeBrowserState* browser_state,
                std::unique_ptr<BrowsingDataRemovalInfo> removal_info);

  // A map that contains the all the ChromeBrowserStates that have a removal
  // operation pending along with their associated BrowsingDataRemovalInfo.
  std::map<ios::ChromeBrowserState*, std::unique_ptr<BrowsingDataRemovalInfo>>
      pending_removals_;
  // The BrowsingDataRemovalInfo of the currently enqueued removal operation.
  std::unique_ptr<BrowsingDataRemovalInfo> current_removal_info_;
  // The actual object that perfoms the removal of browsing data.
  IOSChromeBrowsingDataRemover* current_remover_;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_HELPER_H_
