// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_

@class ContentSuggestion;
@class ContentSuggestionIdentifier;
@class ContentSuggestionsSectionInformation;
@protocol ContentSuggestionsDataSink;
@protocol ContentSuggestionsImageFetcher;

// Typedef for a block taking the fetched suggestions as parameter.
typedef void (^MoreSuggestionsFetched)(NSArray<ContentSuggestion*>* _Nonnull);

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

// Fetches additional content. All the |knownSuggestions| must come from the
// same |sectionInfo|. If the fetch was completed, the given |callback| is
// called with the new content.
- (void)fetchMoreSuggestionsKnowing:
            (nullable NSArray<ContentSuggestionIdentifier*>*)knownSuggestions
                    fromSectionInfo:
                        (nonnull ContentSuggestionsSectionInformation*)
                            sectionInfo
                           callback:(nullable MoreSuggestionsFetched)callback;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_
