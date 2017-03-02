// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_H_
#define IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/base/page_transition_types.h"

class WebStateListObserver;
class WebStateListOrderController;

namespace web {
class WebState;
}

// Manages a list of WebStates. May owns the WebState depending on the ownership
// setting (initialised during construction, should eventually always be "owned"
// once ownership of Tab is sane, see http://crbug.com/546222 for progress).
class WebStateList {
 public:
  enum WebStateOwnership {
    WebStateBorrowed,
    WebStateOwned,
  };

  explicit WebStateList(WebStateOwnership ownership = WebStateBorrowed);
  ~WebStateList();

  // Returns whether the model is empty or not.
  bool empty() const { return web_state_wrappers_.empty(); }

  // Returns the number of WebStates in the model.
  int count() const { return static_cast<int>(web_state_wrappers_.size()); }

  // Returns the index of the currently active WebState, or kInvalidIndex if
  // there are no active WebState.
  int active_index() const { return active_index_; }

  // Returns true if the specified index is contained by the model.
  bool ContainsIndex(int index) const;

  // Returns the currently active WebState or null if there is none.
  web::WebState* GetActiveWebState() const;

  // Returns the WebState at the specified index. It is invalid to call this
  // with an index such that |ContainsIndex(index)| returns false.
  web::WebState* GetWebStateAt(int index) const;

  // Returns the index of the specified WebState or kInvalidIndex if the
  // WebState is not in the model.
  int GetIndexOfWebState(const web::WebState* web_state) const;

  // Returns the WebState that opened the WebState at the specified index or
  // null if there is no opener on record.
  web::WebState* GetOpenerOfWebStateAt(int index) const;

  // Sets the opener for WebState at the specified index. The |opener| must be
  // in the WebStateList.
  void SetOpenerOfWebStateAt(int index, web::WebState* opener);

  // Returns the index of the next WebState in the sequence of WebStates opened
  // from the specified WebState after |start_index|, or kInvalidIndex if there
  // are no such WebState. If |use_group| is true, the opener's navigation index
  // is used to detect navigation changes within the same session.
  int GetIndexOfNextWebStateOpenedBy(const web::WebState* opener,
                                     int start_index,
                                     bool use_group) const;

  // Returns the index of the last WebState in the sequence of WebStates opened
  // from the specified WebState after |start_index|, or kInvalidIndex if there
  // are no such WebState. If |use_group| is true, the opener's navigation index
  // is used to detect navigation changes within the same session.
  int GetIndexOfLastWebStateOpenedBy(const web::WebState* opener,
                                     int start_index,
                                     bool use_group) const;

  // Inserts the specified WebState at the specified index with an optional
  // opener (null if there is no opener).
  void InsertWebState(int index,
                      web::WebState* web_state,
                      web::WebState* opener);

  // Inserts the specified WebState at the best position in the WebStateList
  // given the specified transition, opener (optional, may be null), etc. It
  // defaults to inserting the WebState at the end of the list.
  void AppendWebState(ui::PageTransition transition,
                      web::WebState* web_state,
                      web::WebState* opener);

  // Moves the WebState at the specified index to another index.
  void MoveWebStateAt(int from_index, int to_index);

  // Replaces the WebState at the specified index with new WebState. Returns
  // the old WebState at that index to the caller (abandon ownership of the
  // returned WebState). An optional opener for the new WebState may be passed.
  web::WebState* ReplaceWebStateAt(int index,
                                   web::WebState* web_state,
                                   web::WebState* opener) WARN_UNUSED_RESULT;

  // Detaches the WebState at the specified index. Returns the detached WebState
  // to the caller (abandon ownership of the returned WebState).
  web::WebState* DetachWebStateAt(int index) WARN_UNUSED_RESULT;

  // Makes the WebState at the specified index the active WebState.
  void ActivateWebStateAt(int index);

  // Adds an observer to the model.
  void AddObserver(WebStateListObserver* observer);

  // Removes an observer from the model.
  void RemoveObserver(WebStateListObserver* observer);

  // Invalid index.
  static const int kInvalidIndex = -1;

 private:
  // Sets the opener of any WebState that reference the WebState at the
  // specified index to null.
  void ClearOpenersReferencing(int index);

  // Notify the observers if the active WebState change.
  void NotifyIfActiveWebStateChanged(web::WebState* old_web_state,
                                     bool user_action);

  // Returns the index of the |n|-th WebState (with n > 0) in the sequence of
  // WebStates opened from the specified WebState after |start_index|, or
  // kInvalidIndex if there are no such WebState. If |use_group| is true, the
  // opener's navigation index is used to detect navigation changes within the
  // same session.
  int GetIndexOfNthWebStateOpenedBy(const web::WebState* opener,
                                    int start_index,
                                    bool use_group,
                                    int n) const;

  class WebStateWrapper;
  const WebStateOwnership web_state_ownership_;
  std::vector<std::unique_ptr<WebStateWrapper>> web_state_wrappers_;

  // An object that determines where new WebState should be inserted and where
  // selection should move when a WebState is detached.
  std::unique_ptr<WebStateListOrderController> order_controller_;

  // List of observers notified of changes to the model.
  base::ObserverList<WebStateListObserver, true> observers_;

  // Index of the currently active WebState, kInvalidIndex if no such WebState.
  int active_index_ = kInvalidIndex;

  DISALLOW_COPY_AND_ASSIGN(WebStateList);
};

#endif  // IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_H_
