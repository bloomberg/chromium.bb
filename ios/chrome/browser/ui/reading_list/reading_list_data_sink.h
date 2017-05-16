// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_DATA_SINK_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_DATA_SINK_H_

@protocol ReadingListDataSource;

// Data Sink for the Reading List UI, receiving informations from the data
// source.
@protocol ReadingListDataSink

// Called by the data source when it is ready.
- (void)dataSourceReady:(id<ReadingListDataSource>)dataSource;

// TODO(crbug.com/721758): Group and rename those two methods.
- (void)readingListModelDidApplyChanges;
- (void)readingListModelCompletedBatchUpdates;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_DATA_SINK_H_
