// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_ENTRY_LOADING_UTIL_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_ENTRY_LOADING_UTIL_H_

class ReadingListEntry;
class ReadingListModel;

namespace web {
class WebState;
};

namespace reading_list {

// Loads the URL of the |entry| into the |web_state|. If the entry is
// successfully loaded, marks the entry as read.
void LoadReadingListEntry(ReadingListEntry const& entry,
                          ReadingListModel* model,
                          web::WebState* web_state);

// Loads the distilled URL of the |entry| into the |web_state|, and marks
// |entry| as read. |entry->DistilledState()| must be |PROCESSED|.
void LoadReadingListDistilled(ReadingListEntry const& entry,
                              ReadingListModel* model,
                              web::WebState* web_state);
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_ENTRY_LOADING_UTIL_H_
