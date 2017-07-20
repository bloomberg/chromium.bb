// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_PRINT_OBSERVER_H_
#define IOS_CHROME_BROWSER_WEB_PRINT_OBSERVER_H_

#include "ios/web/public/web_state/web_state_observer.h"

@protocol BrowserCommands;
namespace base {
class DictionaryValue;
}  // namespace base

class GURL;

// Handles print requests from JavaScript window.print.
class PrintObserver : public web::WebStateObserver {
 public:
  PrintObserver(web::WebState* web_state, id<BrowserCommands> dispatcher);
  ~PrintObserver() override;

 private:
  // web::WebStateObserver overrides:
  void WebStateDestroyed() override;

  // Called when print message is sent by the web page.
  bool OnPrintCommand(const base::DictionaryValue&, const GURL&, bool);
  // Stops handling print requests from the web page.
  void Detach();

  __weak id<BrowserCommands> dispatcher_;
};

#endif  // IOS_CHROME_BROWSER_WEB_PRINT_OBSERVER_H_
