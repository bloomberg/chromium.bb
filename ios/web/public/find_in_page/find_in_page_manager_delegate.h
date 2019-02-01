// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_H_
#define IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_H_

#include <string>

namespace web {

class WebState;

class FindInPageManagerDelegate {
 public:
  FindInPageManagerDelegate() = default;

  // Called when a search for |query| finished with |match_count| found after
  // calling FindInPageManager::Find() with FindInPageSearch. Even if no matches
  // are found, call will be made once a find has completed, assuming it has not
  // been interrupted by another find. Client should check |query| to ensure
  // that it is processing |match_count| for the correct find.
  virtual void DidCountMatches(WebState* web_state,
                               int match_count,
                               const std::string& query) = 0;

  // Called when a match number |index| is highlighted. This is triggered by
  // calling FindInPageManager::Find() with any FindInPageOptions to indicate
  // the new match number that was highlighted. This method is not called if
  // |FindInPageManager::Find| did not find any matches.
  virtual void DidHighlightMatch(WebState* web_state, int index) = 0;

 protected:
  virtual ~FindInPageManagerDelegate() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FindInPageManagerDelegate);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_H_
