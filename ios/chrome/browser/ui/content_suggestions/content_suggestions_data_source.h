// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_

@class ContentSuggestion;
@class ContentSuggestionIdentifier;
@class ContentSuggestionsSectionInformation;
@class FaviconAttributes;
@protocol ContentSuggestionsDataSink;
@protocol ContentSuggestionsImageFetcher;
class GURL;

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

// Fetches favicon attributes and calls the completion block.
- (void)fetchFaviconAttributesForURL:(const GURL&)URL
                          completion:
                              (void (^_Nonnull)(FaviconAttributes* _Nonnull))
                                  completion;

// Fetches favicon image associated with this |suggestion| in history. If it is
// not present in the history, tries to download it. Calls the completion block
// if an image has been found.
// This can only be used for public URL.
- (void)
fetchFaviconImageForSuggestion:(nonnull ContentSuggestionIdentifier*)suggestion
                    completion:(void (^_Nonnull)(UIImage* _Nonnull))completion;

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
