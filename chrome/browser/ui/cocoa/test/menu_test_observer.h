// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TEST_MENU_TEST_OBSERVER_H_
#define CHROME_BROWSER_UI_COCOA_TEST_MENU_TEST_OBSERVER_H_

#import <Cocoa/Cocoa.h>

@class MenuTestObserver;
using MenuTestObserverOpenCallback = void (^)(MenuTestObserver*);

// The MenuTestObserver is a helper class for testing around NSMenu. It can
// be used to verify that a menu has or has not been opened, as well as to
// perform some action while the menu is tracking.
@interface MenuTestObserver : NSObject

@property(readonly, nonatomic) NSMenu* menu;

// A flag to indicate whether the menu is currently open.
@property(assign, nonatomic) BOOL isOpen;

// A flag to indicate if the menu has ever been opened.
@property(assign, nonatomic) BOOL didOpen;

// If YES, this test observer will close the menu after it is opened.
@property(assign, nonatomic) BOOL closeAfterOpening;

// An optional block callback to run after the menu has been opened. This will
// be called before closing the menu if |closeAfterOpening| is YES.
@property(copy, nonatomic) MenuTestObserverOpenCallback openCallback;

// Designated initializer. This does not retain the |menu|.
- (instancetype)initWithMenu:(NSMenu*)menu;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TEST_MENU_TEST_OBSERVER_H_
