// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_H_

#import <Foundation/Foundation.h>
#include <memory>

#include "base/macros.h"
#include "base/supports_user_data.h"

@class ChromeBroadcaster;
@class ChromeBroadcastOberverBridge;
class FullscreenControllerObserver;
class FullscreenModel;
class FullscreenWebStateListObserver;

// An object that observes scrolling events in the main content area and
// calculates how much of the toolbar should be visible as a result.  When the
// user scrolls down the screen, the toolbar should be hidden to allow more of
// the page's content to be visible.
class FullscreenController : public base::SupportsUserData::Data {
 public:
  ~FullscreenController() override;

  // Creation and getter functions for FullscreenController.
  // TODO(crbug.com/790886): Convert FullscreenController to a BrowserUserData.
  static void CreateForUserData(base::SupportsUserData* user_data);
  static FullscreenController* FromUserData(base::SupportsUserData* user_data);

  // The ChromeBroadcaster through the FullscreenController receives UI
  // information necessary to calculate fullscreen progress.
  // TODO(crbug.com/790886): Once FullscreenController is a BrowserUserData,
  // remove this ad-hoc broadcaster and drive the animations via the Browser's
  // ChromeBroadcaster.
  ChromeBroadcaster* broadcaster() { return broadcaster_; }

  // Adds and removes FullscreenControllerObservers.
  void AddObserver(FullscreenControllerObserver* observer);
  void RemoveObserver(FullscreenControllerObserver* observer);

  // FullscreenController can be disabled when a feature requires that the
  // toolbar be fully visible.  Since there are multiple reasons fullscreen
  // might need to be disabled, this state is represented by a counter rather
  // than a single bool.  When a feature needs the toolbar to be visible, it
  // calls IncrementDisabledCounter().  After that feature no longer requires
  // the toolbar, it calls DecrementDisabledCounter().  IsEnabled() returns
  // true when the counter is equal to zero.  ScopedFullscreenDisabler can be
  // used to tie a disabled counter to an object's lifetime.
  bool IsEnabled() const;
  void IncrementDisabledCounter();
  void DecrementDisabledCounter();

 private:
  // Private contructor used by CreateForUserData().
  explicit FullscreenController();

  // The broadcaster that drives the model.
  __strong ChromeBroadcaster* broadcaster_ = nil;
  // The model used to calculate fullscreen state.
  std::unique_ptr<FullscreenModel> model_;
  // The bridge used to forward brodcasted UI to |model_|.
  __strong ChromeBroadcastOberverBridge* bridge_ = nil;
  // A WebStateListObserver that updates |model_| for WebStateList changes.
  std::unique_ptr<FullscreenWebStateListObserver> web_state_list_observer_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenController);
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_H_
