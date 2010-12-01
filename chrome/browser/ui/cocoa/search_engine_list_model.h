// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SEARCH_ENGINE_LIST_MODEL_H_
#define CHROME_BROWSER_UI_COCOA_SEARCH_ENGINE_LIST_MODEL_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"

class TemplateURLModel;
class SearchEngineObserver;

// The model for the "default search engine" combobox in preferences. Bridges
// between the cross-platform TemplateURLModel and Cocoa while watching for
// changes to the cross-platform model.

@interface SearchEngineListModel : NSObject {
 @private
  TemplateURLModel* model_;  // weak, owned by Profile
  scoped_ptr<SearchEngineObserver> observer_;  // watches for model changes
  scoped_nsobject<NSArray> engines_;
}

// Initialize with the given template model.
- (id)initWithModel:(TemplateURLModel*)model;

// Returns an array of NSString's corresponding to the user-visible names of the
// search engines.
- (NSArray*)searchEngines;

// The index into |-searchEngines| of the current default search engine. If
// there is no default search engine, the value is -1. The setter changes the
// back-end preference.
- (NSInteger)defaultIndex;
- (void)setDefaultIndex:(NSInteger)index;
// Return TRUE if the default is managed via policy.
- (BOOL)isDefaultManaged;
@end

// Broadcast when the cross-platform model changes. This can be used to update
// any view state that may rely on the position of items in the list.
extern NSString* const kSearchEngineListModelChangedNotification;

#endif  // CHROME_BROWSER_UI_COCOA_SEARCH_ENGINE_LIST_MODEL_H_
