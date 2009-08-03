// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

// By default, NSPopUpButton doesn't allow separator items in its menus. This is
// an NSMenu subclass that automatically adds separator items when presented
// with a @"-" string, so it can be set up in the nib to be the menu class.
@interface AutoseparatingMenu : NSMenu {
}

@end
