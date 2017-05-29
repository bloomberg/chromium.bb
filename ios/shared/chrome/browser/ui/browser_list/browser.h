// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_UI_BROWSER_LIST_BROWSER_H_
#define IOS_SHARED_CHROME_BROWSER_UI_BROWSER_LIST_BROWSER_H_

#include <memory>

#include "base/macros.h"

class WebStateList;
class WebStateListDelegate;

@class ChromeBroadcaster;
@class CommandDispatcher;

namespace ios {
class ChromeBrowserState;
}

// Browser holds the state backing a collection of Tabs and the attached
// UI elements (Tab strip, ...).
class Browser {
 public:
  explicit Browser(ios::ChromeBrowserState* browser_state);
  ~Browser();

  WebStateList& web_state_list() { return *web_state_list_.get(); }
  const WebStateList& web_state_list() const { return *web_state_list_.get(); }

  CommandDispatcher* dispatcher() { return dispatcher_; }

  ios::ChromeBrowserState* browser_state() const { return browser_state_; }

  ChromeBroadcaster* broadcaster() { return broadcaster_; }

 private:
  __strong ChromeBroadcaster* broadcaster_;
  __strong CommandDispatcher* dispatcher_;
  ios::ChromeBrowserState* browser_state_;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

#endif  // IOS_SHARED_CHROME_BROWSER_UI_BROWSER_LIST_BROWSER_H_
