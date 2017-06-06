// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_mediator.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#include "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_data_sink.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
bool EntrySorter(const ReadingListEntry* rhs, const ReadingListEntry* lhs) {
  return rhs->UpdateTime() > lhs->UpdateTime();
}
}  // namespace

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

- (const ReadingListEntry*)entryWithURL:(const GURL&)URL {
  return self.model->GetEntryByURL(URL);
}

- (void)removeEntryWithURL:(const GURL&)URL {
  self.model->RemoveEntryByURL(URL);
}

- (void)fillReadItems:(NSMutableArray<ReadingListCollectionViewItem*>*)readArray
          unreadItems:
              (NSMutableArray<ReadingListCollectionViewItem*>*)unreadArray {
  std::vector<const ReadingListEntry*> readEntries;
  std::vector<const ReadingListEntry*> unreadEntries;

  for (const auto& url : self.model->Keys()) {
    const ReadingListEntry* entry = self.model->GetEntryByURL(url);
    DCHECK(entry);
    if (entry->IsRead()) {
      readEntries.push_back(entry);
    } else {
      unreadEntries.push_back(entry);
    }
  }

  std::sort(readEntries.begin(), readEntries.end(), EntrySorter);
  std::sort(unreadEntries.begin(), unreadEntries.end(), EntrySorter);

  for (const ReadingListEntry* entry : readEntries) {
    [readArray addObject:[self cellItemForReadingListEntry:entry]];
  }

  for (const ReadingListEntry* entry : unreadEntries) {
    [unreadArray addObject:[self cellItemForReadingListEntry:entry]];
  }

  DCHECK(self.model->Keys().size() == [readArray count] + [unreadArray count]);
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

#pragma mark - Private

// Creates a ReadingListCollectionViewItem from a ReadingListEntry |entry|.
- (ReadingListCollectionViewItem*)cellItemForReadingListEntry:
    (const ReadingListEntry*)entry {
  const GURL& url = entry->URL();
  ReadingListCollectionViewItem* item = [[ReadingListCollectionViewItem alloc]
           initWithType:0
                    url:url
      distillationState:reading_list::UIStatusFromModelStatus(
                            entry->DistilledState())];

  item.faviconPageURL =
      entry->DistilledURL().is_valid() ? entry->DistilledURL() : url;

  BOOL has_distillation_details =
      entry->DistilledState() == ReadingListEntry::PROCESSED &&
      entry->DistillationSize() != 0 && entry->DistillationTime() != 0;
  NSString* title = base::SysUTF8ToNSString(entry->Title());
  if ([title length]) {
    item.title = title;
  } else {
    item.title =
        base::SysUTF16ToNSString(url_formatter::FormatUrl(url.GetOrigin()));
  }
  item.subtitle =
      base::SysUTF16ToNSString(url_formatter::FormatUrl(url.GetOrigin()));
  item.distillationDate =
      has_distillation_details ? entry->DistillationTime() : 0;
  item.distillationSize =
      has_distillation_details ? entry->DistillationSize() : 0;
  return item;
}

@end
