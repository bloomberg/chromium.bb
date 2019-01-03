// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_H_

#include "base/macros.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@class NSString;

namespace web {

// Indicates what action the FindinPageManager should take.
enum class FindInPageOptions {
  // Search for a string. Highlight and scroll to the first result if
  // string is found.
  FindInPageSearch = 1,
  // Highlight and scroll to the next result if there is one. Otherwise, nothing
  // will change. Loop back to the first result if currently on last result. If
  // passed before a Find() with FindInPageSearch call, nothing will change.
  FindInPageNext,
  // Highlight and scroll to the previous result if there is one. Otherwise,
  // nothing will change. Loop to last result if currently on first result. If
  // passed before a Find() with FindInPageSearch call, nothing will change.
  FindInPagePrevious,
};

// Manager for searching text on a page. Supports searching within all iframes.
class FindInPageManager : public web::WebStateUserData<FindInPageManager> {
 public:
  // Searches for string |query|. Executes new search or traverses results based
  // on |options|. |query| must not be null if |options| is |FindInPageSearch|.
  // |query| is ignored if |options| is not |FindInPageSearch|.
  virtual void Find(NSString* query, FindInPageOptions options) = 0;

  // Removes any highlighting. Does nothing if Find() with
  // FindInPageOptions::FindInPageSearch is never called.
  virtual void StopFinding() = 0;

  ~FindInPageManager() override {}

 protected:
  FindInPageManager() {}

  DISALLOW_COPY_AND_ASSIGN(FindInPageManager);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_H_
