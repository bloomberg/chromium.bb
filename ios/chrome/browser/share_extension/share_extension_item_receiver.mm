// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/share_extension_item_receiver.h"

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "components/reading_list/ios/reading_list_model_observer.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/web/public/web_thread.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Enum used to send metrics on item reception.
// If you change this enum, update histograms.xml.
enum ShareExtensionItemReceived {
  INVALID_ENTRY = 0,
  CANCELLED_ENTRY,
  READINGLIST_ENTRY,
  BOOKMARK_ENTRY,
  SHARE_EXTENSION_ITEM_RECEIVED_COUNT
};

void LogHistogramReceivedItem(ShareExtensionItemReceived type) {
  UMA_HISTOGRAM_ENUMERATION("IOS.ShareExtension.ReceivedEntry", type,
                            SHARE_EXTENSION_ITEM_RECEIVED_COUNT);
}

}  // namespace

@interface ShareExtensionItemReceiver ()<NSFilePresenter> {
  BOOL _isObservingFolder;
  BOOL _folderCreated;
  ReadingListModel* _readingListModel;  // Not owned.
  bookmarks::BookmarkModel* _bookmarkModel;  // Not owned.
}

// Checks if the reading list folder is already created and if not, create it.
- (void)createReadingListFolder;

// Processes the data sent by the share extension. Data should be a NSDictionary
// serialized by +|NSKeyedArchiver archivedDataWithRootObject:|.
// |completion| is called if |data| has been fully processed.
- (BOOL)receivedData:(NSData*)data withCompletion:(ProceduralBlock)completion;

// Reads the file pointed by |url| and calls |receivedData:| on the content.
// If the file is processed, delete it.
// Must be called on the FILE thread.
// |completion| is only called if the file handling is completed without error.
- (void)handleFileAtURL:(NSURL*)url withCompletion:(ProceduralBlock)completion;

// Deletes the file pointed by |url| then call |completion|.
- (void)deleteFileAtURL:(NSURL*)url withCompletion:(ProceduralBlock)completion;

// Called on UIApplicationDidBecomeActiveNotification notification. Processes
// files that are already in the folder and starts observing the
// app_group::ShareExtensionItemsFolder() folder for new files.
- (void)applicationDidBecomeActive;

// Called on UIApplicationWillResignActiveNotification. Stops observing the
// app_group::ShareExtensionItemsFolder() folder for new files.
- (void)applicationWillResignActive;

// Called whenever a file is modified in app_group::ShareExtensionItemsFolder().
- (void)presentedSubitemDidChangeAtURL:(NSURL*)url;

@end

@implementation ShareExtensionItemReceiver

+ (ShareExtensionItemReceiver*)sharedInstance {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  static ShareExtensionItemReceiver* instance =
      [[ShareExtensionItemReceiver alloc] init];
  return instance;
}

- (void)setBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
        readingListModel:(ReadingListModel*)readingListModel {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(!_readingListModel);
  DCHECK(!_bookmarkModel);

  if (![self presentedItemURL]) {
    return;
  }

  _readingListModel = readingListModel;
  _bookmarkModel = bookmarkModel;

  web::WebThread::PostTask(web::WebThread::FILE, FROM_HERE,
                           base::BindBlockArc(^() {
                             [self createReadingListFolder];
                           }));
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(applicationDidBecomeActive)
             name:UIApplicationDidBecomeActiveNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(applicationWillResignActive)
             name:UIApplicationWillResignActiveNotification
           object:nil];
}

- (void)shutdown {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  if (_isObservingFolder) {
    [NSFileCoordinator removeFilePresenter:self];
  }
  _readingListModel = nil;
}

- (void)dealloc {
  NOTREACHED();
}

- (void)createReadingListFolder {
  DCHECK_CURRENTLY_ON(web::WebThread::FILE);
  if (![[NSFileManager defaultManager]
          fileExistsAtPath:[[self presentedItemURL] path]]) {
    [[NSFileManager defaultManager]
              createDirectoryAtPath:[[self presentedItemURL] path]
        withIntermediateDirectories:NO
                         attributes:nil
                              error:nil];
  }
  web::WebThread::PostTask(
      web::WebThread::UI, FROM_HERE, base::BindBlockArc(^() {
        if ([[UIApplication sharedApplication] applicationState] ==
            UIApplicationStateActive) {
          _folderCreated = YES;
          [self applicationDidBecomeActive];
        }
      }));
}

- (BOOL)receivedData:(NSData*)data withCompletion:(ProceduralBlock)completion {
  id entryID = [NSKeyedUnarchiver unarchiveObjectWithData:data];
  NSDictionary* entry = base::mac::ObjCCast<NSDictionary>(entryID);
  if (!entry) {
    if (completion) {
      completion();
    }
    return NO;
  }

  NSNumber* cancelled = base::mac::ObjCCast<NSNumber>(
      [entry objectForKey:app_group::kShareItemCancel]);
  if (!cancelled) {
    if (completion) {
      completion();
    }
    return NO;
  }
  if ([cancelled boolValue]) {
    LogHistogramReceivedItem(CANCELLED_ENTRY);
    if (completion) {
      completion();
    }
    return YES;
  }

  GURL entryURL =
      net::GURLWithNSURL([entry objectForKey:app_group::kShareItemURL]);
  std::string entryTitle =
      base::SysNSStringToUTF8([entry objectForKey:app_group::kShareItemTitle]);
  NSDate* entryDate = base::mac::ObjCCast<NSDate>(
      [entry objectForKey:app_group::kShareItemDate]);
  NSNumber* entryType = base::mac::ObjCCast<NSNumber>(
      [entry objectForKey:app_group::kShareItemType]);

  if (!entryURL.is_valid() || !entryDate || !entryType ||
      !entryURL.SchemeIsHTTPOrHTTPS()) {
    if (completion) {
      completion();
    }
    return NO;
  }

  UMA_HISTOGRAM_TIMES("IOS.ShareExtension.ReceivedEntryDelay",
                      base::TimeDelta::FromSecondsD(
                          [[NSDate date] timeIntervalSinceDate:entryDate]));

  // Entry is valid. Add it to the reading list model.
  web::WebThread::PostTask(web::WebThread::UI, FROM_HERE,
                           base::BindBlockArc(^() {
                             if (!_readingListModel || !_bookmarkModel) {
                               // Models may have been deleted after the file
                               // processing started.
                               return;
                             }
                             app_group::ShareExtensionItemType type =
                                 static_cast<app_group::ShareExtensionItemType>(
                                     [entryType integerValue]);
                             if (type == app_group::READING_LIST_ITEM) {
                               LogHistogramReceivedItem(READINGLIST_ENTRY);
                               _readingListModel->AddEntry(
                                   entryURL, entryTitle,
                                   reading_list::ADDED_VIA_EXTENSION);
                             }
                             if (type == app_group::BOOKMARK_ITEM) {
                               LogHistogramReceivedItem(BOOKMARK_ENTRY);
                               _bookmarkModel->AddURL(
                                   _bookmarkModel->mobile_node(), 0,
                                   base::UTF8ToUTF16(entryTitle), entryURL);
                             }
                             if (completion) {
                               web::WebThread::PostTask(web::WebThread::FILE,
                                                        FROM_HERE,
                                                        base::BindBlockArc(^() {
                                                          completion();
                                                        }));
                             }
                           }));
  return YES;
}

- (void)handleFileAtURL:(NSURL*)url withCompletion:(ProceduralBlock)completion {
  DCHECK_CURRENTLY_ON(web::WebThread::FILE);
  if (![[NSFileManager defaultManager] fileExistsAtPath:[url path]]) {
    // The handler is called on file modification, including deletion. Check
    // that the file exists before continuing.
    return;
  }
  ProceduralBlock successCompletion = ^{
    DCHECK_CURRENTLY_ON(web::WebThread::FILE);
    [self deleteFileAtURL:url withCompletion:completion];
  };
  NSError* error = nil;
  base::scoped_nsobject<NSFileCoordinator> readingCoordinator(
      [[NSFileCoordinator alloc] initWithFilePresenter:self]);
  [readingCoordinator
      coordinateReadingItemAtURL:url
                         options:NSFileCoordinatorReadingWithoutChanges
                           error:&error
                      byAccessor:^(NSURL* newURL) {
                        NSData* data = [[NSFileManager defaultManager]
                            contentsAtPath:[newURL path]];
                        if (![self receivedData:data
                                 withCompletion:successCompletion]) {
                          LogHistogramReceivedItem(INVALID_ENTRY);
                        }
                      }];
}

- (void)deleteFileAtURL:(NSURL*)url withCompletion:(ProceduralBlock)completion {
  DCHECK_CURRENTLY_ON(web::WebThread::FILE);
  base::scoped_nsobject<NSFileCoordinator> deletingCoordinator(
      [[NSFileCoordinator alloc] initWithFilePresenter:self]);
  NSError* error = nil;
  [deletingCoordinator
      coordinateWritingItemAtURL:url
                         options:NSFileCoordinatorWritingForDeleting
                           error:&error
                      byAccessor:^(NSURL* newURL) {
                        [[NSFileManager defaultManager] removeItemAtURL:newURL
                                                                  error:nil];
                      }];
  if (completion) {
    completion();
  }
}

- (void)applicationDidBecomeActive {
  if (!_folderCreated || _isObservingFolder) {
    return;
  }
  _isObservingFolder = YES;
  // Start observing for new files.
  [NSFileCoordinator addFilePresenter:self];

  // There may already be files. Process them.
  web::WebThread::PostTask(
      web::WebThread::FILE, FROM_HERE, base::BindBlockArc(^() {
        NSArray<NSURL*>* files = [[NSFileManager defaultManager]
              contentsOfDirectoryAtURL:[self presentedItemURL]
            includingPropertiesForKeys:nil
                               options:NSDirectoryEnumerationSkipsHiddenFiles
                                 error:nil];
        if ([files count] == 0) {
          return;
        }
        web::WebThread::PostTask(
            web::WebThread::UI, FROM_HERE, base::BindBlockArc(^() {
              UMA_HISTOGRAM_COUNTS_100(
                  "IOS.ShareExtension.ReceivedEntriesCount", [files count]);
              for (NSURL* fileURL : files) {
                __block std::unique_ptr<
                    ReadingListModel::ScopedReadingListBatchUpdate>
                    batchToken(_readingListModel->BeginBatchUpdates());
                web::WebThread::PostTask(
                    web::WebThread::FILE, FROM_HERE, base::BindBlockArc(^() {
                      [self handleFileAtURL:fileURL
                             withCompletion:^{
                               web::WebThread::PostTask(web::WebThread::UI,
                                                        FROM_HERE,
                                                        base::BindBlockArc(^() {
                                                          batchToken.reset();
                                                        }));
                             }];
                    }));
              }
            }));
      }));
}

- (void)applicationWillResignActive {
  if (!_isObservingFolder) {
    return;
  }
  _isObservingFolder = NO;
  [NSFileCoordinator removeFilePresenter:self];
}

#pragma mark -
#pragma mark NSFilePresenter methods

- (void)presentedSubitemDidChangeAtURL:(NSURL*)url {
  web::WebThread::PostTask(web::WebThread::FILE, FROM_HERE,
                           base::BindBlockArc(^() {
                             [self handleFileAtURL:url withCompletion:nil];
                           }));
}

- (NSOperationQueue*)presentedItemOperationQueue {
  return [NSOperationQueue mainQueue];
}

- (NSURL*)presentedItemURL {
  return app_group::ShareExtensionItemsFolder();
}

@end
