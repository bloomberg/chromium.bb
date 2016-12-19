// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/auto_reload_controller.h"

#include <memory>

#include "base/ios/weak_nsobject.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/timer/timer.h"

namespace {

base::TimeDelta GetAutoReloadTime(size_t reload_count) {
  static const int kDelaysMs[] = {
      0, 5000, 30000, 60000, 300000, 600000, 1800000,
  };
  if (reload_count >= arraysize(kDelaysMs))
    reload_count = arraysize(kDelaysMs) - 1;
  return base::TimeDelta::FromMilliseconds(kDelaysMs[reload_count]);
}

}  // namespace

@interface AutoReloadController () {
  GURL _failedUrl;
  int _reloadCount;
  BOOL _shouldReload;
  BOOL _online;
  id<AutoReloadDelegate> _delegate;
  std::unique_ptr<base::Timer> _timer;
}

// This method is called when the system transitions to offline from online,
// possibly causing a paused reload timer to start.
- (void)transitionedToOffline;

// This method is called when the system transitions from online to offline,
// possibly pausing the reload timer.
- (void)transitionedToOnline;

// This method is called to actually start the reload timer.
- (void)startTimer;

// This method is called back from the reload timer when it fires.
- (void)timerFired;

@end

@implementation AutoReloadController

- (id)initWithDelegate:(id<AutoReloadDelegate>)delegate
          onlineStatus:(BOOL)online {
  if ((self = [super init])) {
    _delegate = delegate;
    _timer.reset(new base::Timer(false /* don't retain user task */,
                                 false /* don't repeat automatically */));
    _online = online;
  }
  return self;
}

- (void)loadStartedForURL:(const GURL&)url {
  if (_timer->IsRunning()) {
    // Stop the timer, since the starting load was not started by this class and
    // it would be bad to replace another load.
    _timer->Stop();
  }
  if (url != _failedUrl) {
    // Changed URLs, reset the backoff counter.
    _reloadCount = 0;
  }
}

- (void)loadFinishedForURL:(const GURL&)url wasPost:(BOOL)wasPost {
  DCHECK(!_timer->IsRunning());
}

- (void)loadFailedForURL:(const GURL&)url wasPost:(BOOL)wasPost {
  DCHECK(!_timer->IsRunning());
  if (wasPost)
    return;
  _failedUrl = url;
  if (_online)
    [self startTimer];
  else
    _shouldReload = true;
}

- (void)networkStateChangedToOnline:(BOOL)online {
  if (!online && _online)
    [self transitionedToOffline];
  else if (online && !_online)
    [self transitionedToOnline];
  _online = online;
}

- (void)setTimerForTesting:(std::unique_ptr<base::Timer>)timer {
  _timer.reset(timer.release());
}

- (void)startTimer {
  base::WeakNSObject<AutoReloadController> weakSelf(self);
  base::TimeDelta delay = GetAutoReloadTime(_reloadCount);
  _timer->Start(FROM_HERE, delay, base::BindBlock(^{
                  base::scoped_nsobject<AutoReloadController> strongSelf(
                      [weakSelf retain]);
                  // self owns the timer owns this closure, so self must outlive
                  // this closure.
                  DCHECK(strongSelf);
                  [strongSelf timerFired];
                }));
}

- (void)transitionedToOffline {
  DCHECK(!_shouldReload);
  if (_timer->IsRunning()) {
    _shouldReload = true;
    _timer->Stop();
  }
}

- (void)transitionedToOnline {
  if (_shouldReload) {
    _shouldReload = false;
    [self startTimer];
  }
}

- (void)timerFired {
  _reloadCount++;
  [_delegate reload];
}

@end
