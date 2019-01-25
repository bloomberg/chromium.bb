// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_
#define IOS_WEB_WEB_STATE_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_

#include <list>
#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#import "ios/web/public/web_state/find_in_page/find_in_page_manager.h"
#include "ios/web/public/web_state/web_state_observer.h"

namespace web {

class WebState;
class WebFrame;

class FindInPageManagerImpl : public FindInPageManager,
                              public web::WebStateObserver {
 public:
  explicit FindInPageManagerImpl(web::WebState* web_state);
  ~FindInPageManagerImpl() override;

  static void CreateForWebState(WebState* web_state);

  // FindInPageManager overrides
  void Find(NSString* query, FindInPageOptions options) override;
  void StopFinding() override;

 private:
  friend class web::WebStateUserData<FindInPageManagerImpl>;
  // Keeps track of the state of an ongoing Find() request.
  struct FindRequest {
    FindRequest();
    ~FindRequest();
    // Unique identifier for each find used to check that it is the most recent
    // find. This ensures that an old find doesn't decrement
    // |pending_frame_calls_count| after it has been reset by the new find.
    int unique_id = 0;
    // Counter to keep track of pending frame JavaScript calls.
    int pending_frame_call_count = 0;
    // Holds number of matches found for each frame keyed by frame_id.
    std::map<std::string, int> frame_match_count;
    // List of frame_ids used for sorting matches.
    std::list<std::string> frame_order;
  };

  // Determines whether find is finished. If not, calls pumpSearch to continue.
  // If it is, calls UpdateFrameMatchesCount(). If find returned null, then does
  // nothing more.
  void ProcessFindInPageResult(const std::string& query,
                               const std::string& frame_id,
                               const int request_id,
                               const base::Value* result);

  // WebStateObserver overrides
  void WebFrameDidBecomeAvailable(WebState* web_state,
                                  WebFrame* web_frame) override;
  void WebFrameWillBecomeUnavailable(WebState* web_state,
                                     WebFrame* web_frame) override;
  void WebStateDestroyed(WebState* web_state) override;

  // Holds the state of the most recent find in page request.
  FindRequest last_find_request_;
  web::WebState* web_state_ = nullptr;
  base::WeakPtrFactory<FindInPageManagerImpl> weak_factory_;
};
}  // namespace web

#endif  // IOS_WEB_WEB_STATE_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_
