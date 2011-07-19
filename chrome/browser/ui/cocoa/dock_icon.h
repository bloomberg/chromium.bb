// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

// A class representing the dock icon of the Chromium app. It's its own class
// since several parts of the app want to manipulate the display of the dock
// icon.
@interface DockIcon : NSObject {
}

+ (DockIcon*)sharedDockIcon;

// Updates the icon. Use the setters below to set the details first.
- (void)updateIcon;

// Download progress ///////////////////////////////////////////////////////////

// Indicates how many downloads are in progress.
- (void)setDownloads:(int)downloads;

// Indicates whether the progress indicator should be in an indeterminate state
// or not.
- (void)setIndeterminate:(BOOL)indeterminate;

// Indicates the amount of progress made of the download. Ranges from [0..1].
- (void)setProgress:(float)progress;

@end
