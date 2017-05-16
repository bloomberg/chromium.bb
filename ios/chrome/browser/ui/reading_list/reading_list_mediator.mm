// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_mediator.h"

#include "base/metrics/histogram_macros.h"
#include "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_data_sink.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ReadingListMediator ()<ReadingListModelBridgeObserver> {
  std::unique_ptr<ReadingListModelBridge> _modelBridge;
}

@property(nonatomic, assign) ReadingListModel* model;

@end

@implementation ReadingListMediator

@synthesize model = _model;
@synthesize dataSink = _dataSink;

#pragma mark - Public

- (instancetype)initWithModel:(ReadingListModel*)model {
  self = [super init];
  if (self) {
    _model = model;

    // This triggers the callback method. Should be created last.
    _modelBridge.reset(new ReadingListModelBridge(self, model));
  }
  return self;
}

#pragma mark - ReadingListDataSource

- (void)dataSinkWillBeDismissed {
  self.model->MarkAllSeen();
  // Reset data sink to prevent further model update notifications.
  self.dataSink = nil;
}

- (void)setReadStatus:(BOOL)read forURL:(const GURL&)URL {
  self.model->SetReadStatus(URL, read);
}

- (const std::vector<GURL>)keys {
  return self.model->Keys();
}

- (const ReadingListEntry*)entryWithURL:(const GURL&)URL {
  return self.model->GetEntryByURL(URL);
}

- (void)removeEntryWithURL:(const GURL&)URL {
  self.model->RemoveEntryByURL(URL);
}

- (std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>)
    beginBatchUpdates {
  return self.model->BeginBatchUpdates();
}

- (BOOL)isPerformingBatchUpdates {
  return self.model->IsPerformingBatchUpdates();
}

#pragma mark - Properties

- (void)setDataSink:(id<ReadingListDataSink>)dataSink {
  _dataSink = dataSink;
  if (self.model->loaded()) {
    [dataSink dataSourceReady:self];
  }
}

- (BOOL)ready {
  return self.model->loaded();
}

- (BOOL)hasElements {
  return self.model->size() > 0;
}

- (BOOL)hasRead {
  return self.model->size() != self.model->unread_size();
}

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  UMA_HISTOGRAM_COUNTS_1000("ReadingList.Unread.Number", model->unread_size());
  UMA_HISTOGRAM_COUNTS_1000("ReadingList.Read.Number",
                            model->size() - model->unread_size());
  [self.dataSink dataSourceReady:self];
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
  [self.dataSink readingListModelDidApplyChanges];
}

- (void)readingListModelCompletedBatchUpdates:(const ReadingListModel*)model {
  [self.dataSink readingListModelCompletedBatchUpdates];
}

@end
