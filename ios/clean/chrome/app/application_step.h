// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_APP_APPLICATION_STEP_H_
#define IOS_CLEAN_CHROME_APP_APPLICATION_STEP_H_

#import <Foundation/Foundation.h>

@class ApplicationState;

// Objects that perform application state change steps must conform to this
// protocol.
@protocol ApplicationStep<NSObject>
// Implementors should not modify |state| in -canRunInState.
- (BOOL)canRunInState:(ApplicationState*)state;

// Implementors should expect to be deallocated after -runInState is called.
// If an implementor creates objects that need to persist longer than that, they
// should be stored in |state.state|.
// Implementors can assume that they are not in any of |state|'s step arrays
// when this method is called.
- (void)runInState:(ApplicationState*)state;
@end

#endif  // IOS_CLEAN_CHROME_APP_APPLICATION_STEP_H_
