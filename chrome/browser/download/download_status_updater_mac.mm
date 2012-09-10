// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include "base/mac/foundation_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/supports_user_data.h"
#include "base/sys_string_conversions.h"
#include "content/public/browser/download_item.h"
#import "chrome/browser/ui/cocoa/dock_icon.h"
#include "googleurl/src/gurl.h"

// --- Private 10.8 API for showing progress ---
// rdar://12058866 http://www.openradar.me/12058866

namespace {

NSString* const kNSProgressAppBundleIdentifierKey =
    @"NSProgressAppBundleIdentifierKey";
NSString* const kNSProgressEstimatedTimeKey =
    @"NSProgressEstimatedTimeKey";
NSString* const kNSProgressFileCompletedCountKey =
    @"NSProgressFileCompletedCountKey";
NSString* const kNSProgressFileContainerURLKey =
    @"NSProgressFileContainerURLKey";
NSString* const kNSProgressFileDownloadingSourceURLKey =
    @"NSProgressFileDownloadingSourceURLKey";
NSString* const kNSProgressFileIconKey =
    @"NSProgressFileIconKey";
NSString* const kNSProgressFileIconOriginalRectKey =
    @"NSProgressFileIconOriginalRectKey";
NSString* const kNSProgressFileLocationCanChangeKey =
    @"NSProgressFileLocationCanChangeKey";
NSString* const kNSProgressFileOperationKindAirDropping =
    @"NSProgressFileOperationKindAirDropping";
NSString* const kNSProgressFileOperationKindCopying =
    @"NSProgressFileOperationKindCopying";
NSString* const kNSProgressFileOperationKindDecompressingAfterDownloading =
    @"NSProgressFileOperationKindDecompressingAfterDownloading";
NSString* const kNSProgressFileOperationKindDownloading =
    @"NSProgressFileOperationKindDownloading";
NSString* const kNSProgressFileOperationKindEncrypting =
    @"NSProgressFileOperationKindEncrypting";
NSString* const kNSProgressFileOperationKindKey =
    @"NSProgressFileOperationKindKey";
NSString* const kNSProgressFileTotalCountKey =
    @"NSProgressFileTotalCountKey";
NSString* const kNSProgressFileURLKey =
    @"NSProgressFileURLKey";
NSString* const kNSProgressIsWaitingKey =
    @"NSProgressIsWaitingKey";
NSString* const kNSProgressKindFile =
    @"NSProgressKindFile";
NSString* const kNSProgressThroughputKey =
    @"NSProgressThroughputKey";

NSString* ProgressString(NSString* string) {
  static NSMutableDictionary* cache;
  static CFBundleRef foundation;
  if (!cache) {
    cache = [[NSMutableDictionary alloc] init];
    foundation = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.Foundation"));
  }

  NSString* result = [cache objectForKey:string];
  if (!result) {
    NSString** ref = static_cast<NSString**>(
        CFBundleGetDataPointerForName(foundation,
                                      base::mac::NSToCFCast(string)));
    if (ref) {
      result = *ref;
      [cache setObject:result forKey:string];
    }
  }

  if (!result) {
    // Huh. At least return a local copy of the expected string.
    result = string;
    NSString* const kKeySuffix = @"Key";
    if ([result hasSuffix:kKeySuffix])
      result = [result substringToIndex:[result length] - [kKeySuffix length]];
  }

  return result;
}

}  // namespace

@interface NSProgress : NSObject

- (id)initWithParent:(id)parent userInfo:(NSDictionary*)info;
@property(copy) NSString* kind;

- (void)unpublish;
- (void)publish;

- (void)setUserInfoObject:(id)object forKey:(NSString*)key;
- (NSDictionary*)userInfo;

@property(readonly) double fractionCompleted;
// Set the totalUnitCount to -1 to indicate an indeterminate download. The dock
// shows a non-filling progress bar; the Finder is lame and draws its progress
// bar off the right side.
@property(readonly, getter=isIndeterminate) BOOL indeterminate;
@property long long completedUnitCount;
@property long long totalUnitCount;

// Pausing appears to be unimplemented in 10.8.0.
- (void)pause;
@property(readonly, getter=isPaused) BOOL paused;
@property(getter=isPausable) BOOL pausable;
- (void)setPausingHandler:(id)blockOfUnknownSignature;

- (void)cancel;
@property(readonly, getter=isCancelled) BOOL cancelled;
@property(getter=isCancellable) BOOL cancellable;
// Note that the cancellation handler block will be called on a random thread.
- (void)setCancellationHandler:(void (^)())block;

// Allows other applications to provide feedback as to whether the progress is
// visible in that app. Note that the acknowledgement handler block will be
// called on a random thread.
// com.apple.dock => BOOL indicating whether the download target folder was
//                   successfully "flown to" at the beginning of the download.
//                   This primarily depends on whether the download target
//                   folder is in the dock. Note that if the download target
//                   folder is added or removed from the dock during the
//                   duration of the download, it will not trigger a callback.
//                   Note that if the "fly to the dock" keys were not set, the
//                   callback's parameter will always be NO.
// com.apple.Finder => always YES, no matter whether the download target
//                     folder's window is open.
- (void)handleAcknowledgementByAppWithBundleIdentifier:(NSString*)bundle
                                       usingBlock:(void (^)(BOOL success))block;

@end

// --- Private 10.8 API for showing progress ---

namespace {

bool NSProgressSupported() {
  static bool supported;
  static bool valid;
  if (!valid) {
    supported = NSClassFromString(@"NSProgress");
    valid = true;
  }

  return supported;
}

const char kCrNSProgressUserDataKey[] = "CrNSProgressUserData";

class CrNSProgressUserData : public base::SupportsUserData::Data {
 public:
  CrNSProgressUserData(NSProgress* progress, const FilePath& target)
      : target_(target) {
    progress_.reset(progress);
  }
  virtual ~CrNSProgressUserData() {
    [progress_.get() unpublish];
  }

  NSProgress* progress() const { return progress_.get(); }
  FilePath target() const { return target_; }
  void setTarget(const FilePath& target) { target_ = target; }

 private:
  scoped_nsobject<NSProgress> progress_;
  FilePath target_;
};

void UpdateAppIcon(int download_count,
                   bool progress_known,
                   float progress) {
  DockIcon* dock_icon = [DockIcon sharedDockIcon];
  [dock_icon setDownloads:download_count];
  [dock_icon setIndeterminate:!progress_known];
  [dock_icon setProgress:progress];
  [dock_icon updateIcon];
}

void CreateNSProgress(content::DownloadItem* download) {
  NSURL* source_url = [NSURL URLWithString:
      base::SysUTF8ToNSString(download->GetURL().possibly_invalid_spec())];
  FilePath destination_path = download->GetFullPath();
  NSURL* destination_url = [NSURL fileURLWithPath:
      base::mac::FilePathToNSString(destination_path)];

  // If there were an image to fly to the download folder in the dock, then
  // the keys in the userInfo to set would be:
  // - @"NSProgressFlyToImageKey" : NSImage
  // - kNSProgressFileIconOriginalRectKey : NSValue of NSRect in global coords

  NSDictionary* user_info = @{
    ProgressString(kNSProgressFileLocationCanChangeKey) : @true,
    ProgressString(kNSProgressFileOperationKindKey) :
        ProgressString(kNSProgressFileOperationKindDownloading),
    ProgressString(kNSProgressFileURLKey) : destination_url
  };

  Class progress_class = NSClassFromString(@"NSProgress");
  NSProgress* progress = [progress_class performSelector:@selector(alloc)];
  progress = [progress performSelector:@selector(initWithParent:userInfo:)
                            withObject:nil
                            withObject:user_info];
  progress.kind = ProgressString(kNSProgressKindFile);

  if (source_url) {
    [progress setUserInfoObject:source_url forKey:
        ProgressString(kNSProgressFileDownloadingSourceURLKey)];
  }

  progress.pausable = NO;
  progress.cancellable = YES;
  [progress setCancellationHandler:^{
      dispatch_async(dispatch_get_main_queue(), ^{
          download->Cancel(true);
      });
  }];

  progress.totalUnitCount = download->GetTotalBytes();
  progress.completedUnitCount = download->GetReceivedBytes();

  [progress publish];

  download->SetUserData(&kCrNSProgressUserDataKey,
                        new CrNSProgressUserData(progress, destination_path));
}

void UpdateNSProgress(content::DownloadItem* download,
                      CrNSProgressUserData* progress_data) {
  NSProgress* progress = progress_data->progress();
  progress.totalUnitCount = download->GetTotalBytes();
  progress.completedUnitCount = download->GetReceivedBytes();

  FilePath download_path = download->GetFullPath();
  if (progress_data->target() != download_path) {
    progress_data->setTarget(download_path);
    NSURL* download_url = [NSURL fileURLWithPath:
        base::mac::FilePathToNSString(download_path)];
    [progress setUserInfoObject:download_url
                         forKey:ProgressString(kNSProgressFileURLKey)];
  }
}

void DestroyNSProgress(content::DownloadItem* download,
                       CrNSProgressUserData* progress_data) {
  download->RemoveUserData(&kCrNSProgressUserDataKey);
}

}  // namespace

void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    content::DownloadItem* download) {

  // Always update overall progress.

  float progress = 0;
  int download_count = 0;
  bool progress_known = GetProgress(&progress, &download_count);
  UpdateAppIcon(download_count, progress_known, progress);

  if (download->GetFullPath().empty())
    return;

  // Update NSProgress-based indicators.

  if (NSProgressSupported()) {
    CrNSProgressUserData* progress_data = static_cast<CrNSProgressUserData*>(
        download->GetUserData(&kCrNSProgressUserDataKey));
    if (!progress_data)
      CreateNSProgress(download);
    else if (download->GetState() != content::DownloadItem::IN_PROGRESS)
      DestroyNSProgress(download, progress_data);
    else
      UpdateNSProgress(download, progress_data);
  }

  // Handle downloads that ended.
  if (download->GetState() != content::DownloadItem::IN_PROGRESS) {
    NSString* download_path =
        base::mac::FilePathToNSString(download->GetFullPath());
    if (download->GetState() == content::DownloadItem::COMPLETE) {
      // Bounce the dock icon.
      [[NSDistributedNotificationCenter defaultCenter]
          postNotificationName:@"com.apple.DownloadFileFinished"
                        object:download_path];
    }

    // Notify the Finder.
    NSString* parent_path = [download_path stringByDeletingLastPathComponent];
    FNNotifyByPath(
        reinterpret_cast<const UInt8*>([parent_path fileSystemRepresentation]),
        kFNDirectoryModifiedMessage,
        kNilOptions);
  }
}
