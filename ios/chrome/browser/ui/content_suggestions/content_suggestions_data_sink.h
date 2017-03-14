// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SINK_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SINK_H_

@class ContentSuggestionIdentifier;
@class ContentSuggestionsSectionInformation;

// Data sink for the ContentSuggestions. It will be notified when new data needs
// to be pulled.
@protocol ContentSuggestionsDataSink

// Notifies the Data Sink that new data is available for the section identified
// by |sectionInfo|.
- (void)dataAvailableForSection:
    (ContentSuggestionsSectionInformation*)sectionInfo;

// The suggestion associated with |suggestionIdentifier| has been invalidated by
// the backend data source and should be cleared now. This is why this method is
// about the data source pushing something to the data sink.
- (void)clearSuggestion:(ContentSuggestionIdentifier*)suggestionIdentifier;

// Notifies the Data Sink that it must remove all current data and reload new
// ones.
- (void)reloadAllData;

// The section corresponding to |sectionInfo| has been invalidated and must be
// cleared now. This is why this method is about the data source pushing
// something to the data sink.
- (void)clearSection:(ContentSuggestionsSectionInformation*)sectionInfo;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SINK_H_
