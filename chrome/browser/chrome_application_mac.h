// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_APPLICATION_MAC_H_
#define CHROME_BROWSER_CHROME_APPLICATION_MAC_H_

#ifdef __OBJC__

#import <AppKit/AppKit.h>

@interface CrApplication : NSApplication
@end

// Namespace for exception-reporting helper functions.  Exposed for
// testing purposes.
namespace CrApplicationNSException {

// Bin for unknown exceptions.
extern const size_t kUnknownNSException;

// Returns the histogram bin for |exception| if it is one we track
// specifically, or |kUnknownNSException| if unknown.
size_t BinForException(NSException* exception);

// Use UMA to track exception occurance.
void RecordExceptionWithUma(NSException* exception);

}  // CrApplicationNSException

#endif  // __OBJC__

// CrApplicationCC provides access to CrApplication Objective-C selectors from
// C++ code.
namespace CrApplicationCC {

// Calls -[NSApp terminate:].
void Terminate();

}  // namespace CrApplicationCC

#endif  // CHROME_BROWSER_CHROME_APPLICATION_MAC_H_
