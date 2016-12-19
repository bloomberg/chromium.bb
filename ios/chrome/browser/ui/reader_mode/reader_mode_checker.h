// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_CHECKER_H_
#define IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_CHECKER_H_

#import <Foundation/Foundation.h>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ios/web/public/web_state/web_state_observer.h"

namespace web {
class WebState;
}

class ReaderModeChecker;

@class CRWJSInjectionReceiver;

class ReaderModeCheckerObserver {
 public:
  explicit ReaderModeCheckerObserver(ReaderModeChecker* readerModeChecker);
  virtual ~ReaderModeCheckerObserver();

  // Called when the content of the page is known to be distillable.
  virtual void PageIsDistillable() = 0;

  // Invoked when the ReaderModeChecker is being destroyed. Gives subclasses a
  // chance to cleanup.
  virtual void ReaderModeCheckerDestroyed() {}

 private:
  ReaderModeChecker* readerModeChecker_;
  DISALLOW_COPY_AND_ASSIGN(ReaderModeCheckerObserver);
};

// Checks if a page loaded in a container that allows javascript execution is
// suitable for reader mode.
class ReaderModeChecker : web::WebStateObserver {
 public:
  explicit ReaderModeChecker(web::WebState* web_state);
  ~ReaderModeChecker() override;

  // Returns the switch status of the current page.
  bool CanSwitchToReaderMode();

  // web::WebStateObserver.
  void NavigationItemCommitted(
      const web::LoadCommittedDetails& load_details) override;
  void PageLoaded(
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed() override;

  // Adds and removes observers for page navigation notifications. The order in
  // which notifications are sent to observers is undefined. Clients must be
  // sure to remove the observer before they go away.
  void AddObserver(ReaderModeCheckerObserver* observer);
  void RemoveObserver(ReaderModeCheckerObserver* observer);

 private:
  // Checks if the current webstate is distillable.
  void CheckIsDistillable();
  // Does a simple check, Just checking OpenGraph properties.
  void CheckIsDistillableOG(CRWJSInjectionReceiver* receiver);
  // Does a more involved check.
  void CheckIsDistillableDetector(CRWJSInjectionReceiver* receiver);
  // Stores if the current webstate URL is suitable for reader mode.
  enum class Status;
  Status is_distillable_;
  // A list of observers notified when page state changes. Weak references.
  base::ObserverList<ReaderModeCheckerObserver, true> observers_;
  base::WeakPtrFactory<ReaderModeChecker> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(ReaderModeChecker);
};

#endif  // IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_CHECKER_H_
