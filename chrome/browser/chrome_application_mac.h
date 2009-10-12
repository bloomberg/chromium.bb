// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_APPLICATION_MAC_H_
#define CHROME_BROWSER_CHROME_APPLICATION_MAC_H_

#ifdef __OBJC__

#import <AppKit/AppKit.h>

@interface CrApplication : NSApplication
@end

#endif  // __OBJC__

// CrApplicationCC provides access to CrApplication Objective-C selectors from
// C++ code.
namespace CrApplicationCC {

// Calls -[NSApp terminate:].
void Terminate();

}  // namespace CrApplicationCC

#endif  // CHROME_BROWSER_CHROME_APPLICATION_MAC_H_
