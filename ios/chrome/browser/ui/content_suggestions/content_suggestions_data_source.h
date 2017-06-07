// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

@class CollectionViewItem;
@class ContentSuggestion;
@class ContentSuggestionIdentifier;
@class ContentSuggestionsSectionInformation;
@class FaviconAttributes;
@protocol ContentSuggestionsDataSink;
@protocol ContentSuggestionsImageFetcher;
@protocol SuggestedContent;

// Typedef for a block taking the fetched suggestions as parameter.
typedef void (^MoreSuggestionsFetched)(
    NSArray<CollectionViewItem<SuggestedContent>*>* _Nullable);

// DataSource for the content suggestions. Provides the suggestions data in a
// format compatible with Objective-C.
@protocol ContentSuggestionsDataSource

// The data sink that will be notified when the data change.
@property(nonatomic, nullable, weak) id<ContentSuggestionsDataSink> dataSink;

// Returns all the sections information in the order they should be displayed.
- (nonnull NSArray<ContentSuggestionsSectionInformation*>*)sectionsInfo;

// Returns the items associated with the |sectionInfo|.
- (nonnull NSArray<CollectionViewItem<SuggestedContent>*>*)itemsForSectionInfo:
    (nonnull ContentSuggestionsSectionInformation*)sectionInfo;

// Returns an image updater for the suggestions provided by this data source.
- (nullable id<ContentSuggestionsImageFetcher>)imageFetcher;

// Fetches favicon attributes associated with the |item| and calls the
// |completion| block.
- (void)fetchFaviconAttributesForItem:
            (nonnull CollectionViewItem<SuggestedContent>*)item
                           completion:
                               (void (^_Nonnull)(FaviconAttributes* _Nonnull))
                                   completion;

// Fetches the favicon image associated with the |item|. If there is no image
// cached locally and the provider allows it, it tries to download it. The
// |completion| block is only called if an image is found.
- (void)
fetchFaviconImageForItem:(nonnull CollectionViewItem<SuggestedContent>*)item
              completion:(void (^_Nonnull)(UIImage* _Nonnull))completion;

// Fetches additional content. All the |knownSuggestions| must come from the
// same |sectionInfo|. If the fetch was completed, the given |callback| is
// called with the new content.
- (void)fetchMoreSuggestionsKnowing:
            (nullable NSArray<ContentSuggestionIdentifier*>*)knownSuggestions
                    fromSectionInfo:
                        (nonnull ContentSuggestionsSectionInformation*)
                            sectionInfo
                           callback:(nonnull MoreSuggestionsFetched)callback;

// Dismisses the suggestion from the content suggestions service. It doesn't
// change the UI.
- (void)dismissSuggestion:
    (nonnull ContentSuggestionIdentifier*)suggestionIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_
