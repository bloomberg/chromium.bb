// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_H_
#define IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"

class WebStateListObserver;

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

  // Returns true if the specified index is contained by the model.
  bool ContainsIndex(int index) const;

  // Returns the WebState at the specified index. It is invalid to call this
  // with an index such that |ContainsIndex(index)| returns false.
  web::WebState* GetWebStateAt(int index) const;

  // Returns the index of the specified WebState or kInvalidIndex if the
  // WebState is not in the model.
  int GetIndexOfWebState(const web::WebState* web_state) const;

  // Inserts the specified WebState at the specified index.
  void InsertWebState(int index, web::WebState* web_state);

  // Moves the WebState at the specified index to another index.
  void MoveWebStateAt(int from_index, int to_index);

  // Replaces the WebState at the specified index with new WebState. Returns
  // the old WebState at that index to the caller (abandon ownership of the
  // returned WebState).
  web::WebState* ReplaceWebStateAt(int index, web::WebState* web_state);

  // Detaches the WebState at the specified index.
  void DetachWebStateAt(int index);

  // Adds an observer to the model.
  void AddObserver(WebStateListObserver* observer);

  // Removes an observer from the model.
  void RemoveObserver(WebStateListObserver* observer);

  // Invalid index.
  static const int kInvalidIndex = -1;

 private:
  class WebStateWrapper;
  const WebStateOwnership web_state_ownership_;
  std::vector<std::unique_ptr<WebStateWrapper>> web_state_wrappers_;

  // List of observers notified of changes to the model.
  base::ObserverList<WebStateListObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(WebStateList);
};

#endif  // IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_H_
