// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_OBSERVER_H_
#define IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_OBSERVER_H_

#include "base/macros.h"

class WebStateList;

namespace web {
class WebState;
}

// Interface for listening to events occurring to WebStateLists.
class WebStateListObserver {
 public:
  WebStateListObserver();
  virtual ~WebStateListObserver();

  // Invoked after a new WebState has been added to the WebStateList at the
  // specified index.
  virtual void WebStateInsertedAt(WebStateList* web_state_list,
                                  web::WebState* web_state,
                                  int index);

  // Invoked after the WebState at the specified index is moved to another
  // index.
  virtual void WebStateMoved(WebStateList* web_state_list,
                             web::WebState* web_state,
                             int from_index,
                             int to_index);

  // Invoked after the WebState at the specified index is replaced by another
  // WebState.
  virtual void WebStateReplacedAt(WebStateList* web_state_list,
                                  web::WebState* old_web_state,
                                  web::WebState* new_web_state,
                                  int index);

  // Invoked after the WebState at the specified index has been detached. The
  // WebState is still valid but is no longer in the WebStateList.
  virtual void WebStateDetachedAt(WebStateList* web_state_list,
                                  web::WebState* web_state,
                                  int index);

  // Invoked after |new_web_state| was activated at the specified index. Both
  // WebState are either valid or null (if there was no selection or there is
  // no selection). If the change is due to an user action, |user_action| will
  // be true.
  virtual void WebStateActivatedAt(WebStateList* web_state_list,
                                   web::WebState* old_web_state,
                                   web::WebState* new_web_state,
                                   int active_index,
                                   bool user_action);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebStateListObserver);
};

#endif  // IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_OBSERVER_H_
