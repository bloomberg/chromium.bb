// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SPINNER_PROGRESS_INDICATOR_
#define CHROME_BROWSER_UI_COCOA_SPINNER_PROGRESS_INDICATOR_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "base/time.h"

namespace base {
class Timer;
}

// A progress indicator that draws progress as a pie chart. An terminate
// state is represented by simply spinning a slice of the pie around the
// control.
@interface SpinnerProgressIndicator : NSView {
 @private
  scoped_ptr<base::Timer> timer_;
  base::TimeTicks startTime_;
  int percentDone_;
  BOOL isIndeterminate_;
}

@property(nonatomic, assign) int percentDone;
@property(nonatomic, assign) BOOL isIndeterminate;

- (void)sizeToFit;

@end

#endif  // CHROME_BROWSER_UI_COCOA_SPINNER_PROGRESS_INDICATOR_
