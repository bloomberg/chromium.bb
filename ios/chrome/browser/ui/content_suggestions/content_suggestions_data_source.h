// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_

@class ContentSuggestion;
@class ContentSuggestionsSectionInformation;
@protocol ContentSuggestionsDataSink;
@protocol ContentSuggestionsImageFetcher;

// DataSource for the content suggestions. Provides the suggestions data in a
// format compatible with Objective-C.
@protocol ContentSuggestionsDataSource

// The data sink that will be notified when the data change.
@property(nonatomic, nullable, weak) id<ContentSuggestionsDataSink> dataSink;

// Returns all the data currently available.
- (nonnull NSArray<ContentSuggestion*>*)allSuggestions;

// Returns the data currently available for the section identified by
// |sectionInfo|.
- (nonnull NSArray<ContentSuggestion*>*)suggestionsForSection:
    (nonnull ContentSuggestionsSectionInformation*)sectionInfo;

// Returns an image updater for the suggestions provided by this data source.
- (nullable id<ContentSuggestionsImageFetcher>)imageFetcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_
