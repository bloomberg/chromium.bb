// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_CUSTOM_HOME_PAGES_MODEL_H_
#define CHROME_BROWSER_COCOA_CUSTOM_HOME_PAGES_MODEL_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include <vector>
#include "base/scoped_nsobject.h"
#include "chrome/browser/history/history.h"
#include "googleurl/src/gurl.h"

class Profile;

// The model for the "custom home pages" table in preferences. Contains a list
// of CustomHomePageEntry objects. This is intended to be used with Cocoa
// bindings.
//
// The supported binding is |customHomePages|, a to-many relationship which
// can be observed with an array controller.

@interface CustomHomePagesModel : NSObject {
 @private
  scoped_nsobject<NSMutableArray> entries_;
  Profile* profile_;  // weak, used for loading favicons
}

// Initialize with |profile|, which must not be NULL. The profile is used for
// loading favicons for urls.
- (id)initWithProfile:(Profile*)profile;

// Get/set the urls the model currently contains as a group. Only one change
// notification will be sent.
- (std::vector<GURL>)URLs;
- (void)setURLs:(const std::vector<GURL>&)urls;

// Reloads the URLs from their stored state. This will notify using KVO
// |customHomePages|.
- (void)reloadURLs;

// Validates the set of URLs stored in the model. The user may have input bad
// data. This function removes invalid entries from the model, which will result
// in anyone observing being updated.
- (void)validateURLs;

// For binding |customHomePages| to a mutable array controller.
- (NSUInteger)countOfCustomHomePages;
- (id)objectInCustomHomePagesAtIndex:(NSUInteger)index;
- (void)insertObject:(id)object inCustomHomePagesAtIndex:(NSUInteger)index;
- (void)removeObjectFromCustomHomePagesAtIndex:(NSUInteger)index;
@end

////////////////////////////////////////////////////////////////////////////////

// An entry representing a single item in the custom home page model. Stores
// a url and a favicon.
@interface CustomHomePageEntry : NSObject {
 @private
  scoped_nsobject<NSString> url_;
  scoped_nsobject<NSImage> icon_;

  // If non-zero, indicates we're loading the favicon for the page.
  HistoryService::Handle icon_handle_;
}

@property(nonatomic, copy) NSString* URL;
@property(nonatomic, retain) NSImage* image;

@end

////////////////////////////////////////////////////////////////////////////////

@interface CustomHomePagesModel (InternalOrTestingAPI)

// Clears the URL string at the specified index. This constitutes bad data. The
// validator should scrub the entry from the list the next time it is run.
- (void)setURLStringEmptyAt:(NSUInteger)index;

@end

// A notification that fires when the URL of one of the entries changes.
// Prevents interested parties from having to observe all model objects in order
// to persist changes to a single entry. Changes to the number of items in the
// model can be observed by watching |customHomePages| via KVO so an additional
// notification is not sent.
extern NSString* const kHomepageEntryChangedNotification;

#endif  // CHROME_BROWSER_COCOA_CUSTOM_HOME_PAGES_MODEL_H_
