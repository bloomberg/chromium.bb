// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_cache.h"

#import <UIKit/UIKit.h>

#include "base/critical_closure.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/snapshots/lru_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_internal.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/web/public/web_thread.h"

@interface SnapshotCache ()
// Returns the directory where the thumbnails are saved.
+ (base::FilePath)cacheDirectory;
// Returns the directory where the thumbnails were stored in M28 and earlier.
- (base::FilePath)oldCacheDirectory;
// Remove all UIImages from |imageDictionary_|.
- (void)handleEnterBackground;
// Remove all but adjacent UIImages from |imageDictionary_|.
- (void)handleLowMemory;
// Restore adjacent UIImages to |imageDictionary_|.
- (void)handleBecomeActive;
// Clear most recent caller information.
- (void)clearGreySessionInfo;
// Load uncached snapshot image and convert image to grey.
- (void)loadGreyImageAsync:(NSString*)sessionID;
// Save grey image to |greyImageDictionary_| and call into most recent
// |mostRecentGreyBlock_| if |mostRecentGreySessionId_| matches |sessionID|.
- (void)saveGreyImage:(UIImage*)greyImage forKey:(NSString*)sessionID;
@end

namespace {
static NSArray* const kSnapshotCacheDirectory = @[ @"Chromium", @"Snapshots" ];

const NSUInteger kCacheInitialCapacity = 100;
const NSUInteger kGreyInitialCapacity = 8;
const CGFloat kJPEGImageQuality = 1.0;  // Highest quality. No compression.
// Sequence token to make sure creation/deletion of snapshots don't overlap.
const char kSequenceToken[] = "SnapshotCacheSequenceToken";
// Maximum size in number of elements that the LRU cache can hold before
// starting to evict elements.
const NSUInteger kLRUCacheMaxCapacity = 6;

// The paths of the images saved to disk, given a cache directory.
base::FilePath FilePathForSessionID(NSString* sessionID,
                                    const base::FilePath& directory) {
  base::FilePath path = directory.Append(base::SysNSStringToUTF8(sessionID))
                            .ReplaceExtension(".jpg");
  if ([SnapshotCache snapshotScaleForDevice] == 2.0) {
    path = path.InsertBeforeExtension("@2x");
  } else if ([SnapshotCache snapshotScaleForDevice] == 3.0) {
    path = path.InsertBeforeExtension("@3x");
  }
  return path;
}

base::FilePath GreyFilePathForSessionID(NSString* sessionID,
                                        const base::FilePath& directory) {
  base::FilePath path = directory.Append(base::SysNSStringToUTF8(sessionID) +
                                         "Grey").ReplaceExtension(".jpg");
  if ([SnapshotCache snapshotScaleForDevice] == 2.0) {
    path = path.InsertBeforeExtension("@2x");
  } else if ([SnapshotCache snapshotScaleForDevice] == 3.0) {
    path = path.InsertBeforeExtension("@3x");
  }
  return path;
}

UIImage* ReadImageFromDisk(const base::FilePath& filePath) {
  base::ThreadRestrictions::AssertIOAllowed();
  // TODO(justincohen): Consider changing this back to -imageWithContentsOfFile
  // instead of -imageWithData, if the crashing rdar://15747161 is ever fixed.
  // Tracked in crbug.com/295891.
  NSString* path = base::SysUTF8ToNSString(filePath.value());
  return [UIImage imageWithData:[NSData dataWithContentsOfFile:path]
                          scale:[SnapshotCache snapshotScaleForDevice]];
}

void WriteImageToDisk(const base::scoped_nsobject<UIImage>& image,
                      const base::FilePath& filePath) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (!image)
    return;
  NSString* path = base::SysUTF8ToNSString(filePath.value());
  [UIImageJPEGRepresentation(image, kJPEGImageQuality) writeToFile:path
                                                        atomically:YES];
  // Encrypt the snapshot file (mostly for Incognito, but can't hurt to
  // always do it).
  NSDictionary* attributeDict =
      [NSDictionary dictionaryWithObject:NSFileProtectionComplete
                                  forKey:NSFileProtectionKey];
  NSError* error = nil;
  BOOL success = [[NSFileManager defaultManager] setAttributes:attributeDict
                                                  ofItemAtPath:path
                                                         error:&error];
  if (!success) {
    DLOG(ERROR) << "Error encrypting thumbnail file"
                << base::SysNSStringToUTF8([error description]);
  }
}

void ConvertAndSaveGreyImage(
    const base::FilePath& colorPath,
    const base::FilePath& greyPath,
    const base::scoped_nsobject<UIImage>& cachedImage) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::scoped_nsobject<UIImage> colorImage = cachedImage;
  if (!colorImage)
    colorImage.reset([ReadImageFromDisk(colorPath) retain]);
  if (!colorImage)
    return;
  base::scoped_nsobject<UIImage> greyImage([GreyImage(colorImage) retain]);
  WriteImageToDisk(greyImage, greyPath);
}

}  // anonymous namespace

@implementation SnapshotCache {
  // Dictionary to hold color snapshots in memory. n.b. Color snapshots are not
  // kept in memory on tablets.
  base::scoped_nsobject<NSMutableDictionary> imageDictionary_;

  // Cache to hold color snapshots in memory. n.b. Color snapshots are not
  // kept in memory on tablets. It is used in place of the imageDictionary_ when
  // the LRU cache snapshot experiment is enabled.
  base::scoped_nsobject<LRUCache> lruCache_;

  // Temporary dictionary to hold grey snapshots for tablet side swipe. This
  // will be nil before -createGreyCache is called and after -removeGreyCache
  // is called.
  base::scoped_nsobject<NSMutableDictionary> greyImageDictionary_;
  NSSet* pinnedIDs_;

  // Session ID of most recent pending grey snapshot request.
  base::scoped_nsobject<NSString> mostRecentGreySessionId_;
  // Block used by pending request for a grey snapshot.
  base::scoped_nsprotocol<GreyBlock> mostRecentGreyBlock_;

  // Session ID and correspoinding UIImage for the snapshot that will likely
  // be requested to be saved to disk when the application is backgrounded.
  base::scoped_nsobject<NSString> backgroundingImageSessionId_;
  base::scoped_nsobject<UIImage> backgroundingColorImage_;

  base::mac::ObjCPropertyReleaser propertyReleaser_SnapshotCache_;
}

@synthesize pinnedIDs = pinnedIDs_;

+ (SnapshotCache*)sharedInstance {
  static SnapshotCache* instance = [[SnapshotCache alloc] init];
  return instance;
}

- (id)init {
  if ((self = [super init])) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    propertyReleaser_SnapshotCache_.Init(self, [SnapshotCache class]);

    if ([self usesLRUCache]) {
      lruCache_.reset(
          [[LRUCache alloc] initWithCacheSize:kLRUCacheMaxCapacity]);
    } else {
      imageDictionary_.reset(
          [[NSMutableDictionary alloc] initWithCapacity:kCacheInitialCapacity]);
    }
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleLowMemory)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleEnterBackground)
               name:UIApplicationDidEnterBackgroundNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleBecomeActive)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidReceiveMemoryWarningNotification
              object:nil];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidEnterBackgroundNotification
              object:nil];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:nil];
  [super dealloc];
}

+ (CGFloat)snapshotScaleForDevice {
  // On handset, the color snapshot is used for the stack view, so the scale of
  // the snapshot images should match the scale of the device.
  // On tablet, the color snapshot is only used to generate the grey snapshot,
  // which does not have to be high quality, so use scale of 1.0 on all tablets.
  if (IsIPadIdiom()) {
    return 1.0;
  }
  // Cap snapshot resolution to 2x to reduce the amount of memory they use.
  return MIN([UIScreen mainScreen].scale, 2.0);
}

- (void)retrieveImageForSessionID:(NSString*)sessionID
                         callback:(void (^)(UIImage*))callback {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(sessionID);

  UIImage* img = nil;
  if (lruCache_)
    img = [lruCache_ objectForKey:sessionID];
  else
    img = [imageDictionary_ objectForKey:sessionID];

  if (img) {
    if (callback)
      callback(img);
    return;
  }

  base::PostTaskAndReplyWithResult(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE_USER_BLOCKING)
          .get(),
      FROM_HERE, base::BindBlock(^base::scoped_nsobject<UIImage>() {
        // Retrieve the image on a high priority thread.
        return base::scoped_nsobject<UIImage>([ReadImageFromDisk(
            [SnapshotCache imagePathForSessionID:sessionID]) retain]);
      }),
      base::BindBlock(^(base::scoped_nsobject<UIImage> image) {
        if (image) {
          if (lruCache_)
            [lruCache_ setObject:image forKey:sessionID];
          else
            [imageDictionary_ setObject:image forKey:sessionID];
        }
        if (callback)
          callback(image);
      }));
}

- (void)setImage:(UIImage*)img withSessionID:(NSString*)sessionID {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (!img || !sessionID)
    return;

  if (lruCache_)
    [lruCache_ setObject:img forKey:sessionID];
  else
    [imageDictionary_ setObject:img forKey:sessionID];

  // Save the image to disk.
  web::WebThread::PostBlockingPoolSequencedTask(
      kSequenceToken, FROM_HERE,
      base::BindBlock(^{
        base::scoped_nsobject<UIImage> image([img retain]);
        WriteImageToDisk(image,
                         [SnapshotCache imagePathForSessionID:sessionID]);
      }));
}

- (void)removeImageWithSessionID:(NSString*)sessionID {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (lruCache_)
    [lruCache_ removeObjectForKey:sessionID];
  else
    [imageDictionary_ removeObjectForKey:sessionID];

  web::WebThread::PostBlockingPoolSequencedTask(
      kSequenceToken, FROM_HERE,
      base::BindBlock(^{
        base::FilePath imagePath =
            [SnapshotCache imagePathForSessionID:sessionID];
        base::DeleteFile(imagePath, false);
        base::DeleteFile([SnapshotCache greyImagePathForSessionID:sessionID],
                         false);
      }));
}

- (base::FilePath)oldCacheDirectory {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,
                                                       NSUserDomainMask, YES);
  NSString* path = [paths objectAtIndex:0];
  NSArray* path_components =
      [NSArray arrayWithObjects:path, kSnapshotCacheDirectory[1], nil];
  return base::FilePath(
      base::SysNSStringToUTF8([NSString pathWithComponents:path_components]));
}

+ (base::FilePath)cacheDirectory {
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,
                                                       NSUserDomainMask, YES);
  NSString* path = [paths objectAtIndex:0];
  NSArray* path_components =
      [NSArray arrayWithObjects:path, kSnapshotCacheDirectory[0],
                                kSnapshotCacheDirectory[1], nil];
  return base::FilePath(
      base::SysNSStringToUTF8([NSString pathWithComponents:path_components]));
}

+ (base::FilePath)imagePathForSessionID:(NSString*)sessionID {
  base::ThreadRestrictions::AssertIOAllowed();

  base::FilePath path([SnapshotCache cacheDirectory]);

  BOOL exists = base::PathExists(path);
  DCHECK(base::DirectoryExists(path) || !exists);
  if (!exists) {
    bool result = base::CreateDirectory(path);
    DCHECK(result);
  }
  return FilePathForSessionID(sessionID, path);
}

+ (base::FilePath)greyImagePathForSessionID:(NSString*)sessionID {
  base::ThreadRestrictions::AssertIOAllowed();

  base::FilePath path([self cacheDirectory]);

  BOOL exists = base::PathExists(path);
  DCHECK(base::DirectoryExists(path) || !exists);
  if (!exists) {
    bool result = base::CreateDirectory(path);
    DCHECK(result);
  }
  return GreyFilePathForSessionID(sessionID, path);
}

- (void)purgeCacheOlderThan:(const base::Time&)date
                    keeping:(NSSet*)liveSessionIds {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  // Copying the date, as the block must copy the value, not the reference.
  const base::Time dateCopy = date;
  web::WebThread::PostBlockingPoolSequencedTask(
      kSequenceToken, FROM_HERE,
      base::BindBlock(^{
        std::set<base::FilePath> filesToKeep;
        for (NSString* sessionID : liveSessionIds) {
          base::FilePath curImagePath =
              [SnapshotCache imagePathForSessionID:sessionID];
          filesToKeep.insert(curImagePath);
          filesToKeep.insert(
              [SnapshotCache greyImagePathForSessionID:sessionID]);
        }
        base::FileEnumerator enumerator([SnapshotCache cacheDirectory], false,
                                        base::FileEnumerator::FILES);
        base::FilePath cur_file;
        while (!(cur_file = enumerator.Next()).value().empty()) {
          if (cur_file.Extension() != ".jpg")
            continue;
          if (filesToKeep.find(cur_file) != filesToKeep.end()) {
            continue;
          }
          base::FileEnumerator::FileInfo fileInfo = enumerator.GetInfo();
          if (fileInfo.GetLastModifiedTime() > dateCopy) {
            continue;
          }
          base::DeleteFile(cur_file, false);
        }
      }));
}

- (void)willBeSavedGreyWhenBackgrounding:(NSString*)sessionID {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (!sessionID)
    return;
  backgroundingImageSessionId_.reset([sessionID copy]);
  if (lruCache_) {
    backgroundingColorImage_.reset([[lruCache_ objectForKey:sessionID] retain]);
  } else {
    backgroundingColorImage_.reset(
        [[imageDictionary_ objectForKey:sessionID] retain]);
  }
}

- (BOOL)usesLRUCache {
  // TODO(crbug.com/687904): Remove the non-LRU cache code.
  return true;
}

- (void)handleLowMemory {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  base::scoped_nsobject<NSMutableDictionary> dictionary(
      [[NSMutableDictionary alloc] initWithCapacity:2]);
  for (NSString* sessionID in pinnedIDs_) {
    UIImage* image = nil;
    if (lruCache_)
      image = [lruCache_ objectForKey:sessionID];
    else
      image = [imageDictionary_ objectForKey:sessionID];
    if (image)
      [dictionary setObject:image forKey:sessionID];
  }
  if (lruCache_) {
    [lruCache_ removeAllObjects];
    for (NSString* sessionID in pinnedIDs_)
      [lruCache_ setObject:[dictionary objectForKey:sessionID]
                    forKey:sessionID];
  } else {
    imageDictionary_ = dictionary;
  }
}

- (void)handleEnterBackground {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [imageDictionary_ removeAllObjects];
  [lruCache_ removeAllObjects];
}

- (void)handleBecomeActive {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  for (NSString* sessionID in pinnedIDs_)
    [self retrieveImageForSessionID:sessionID callback:nil];
}

- (void)saveGreyImage:(UIImage*)greyImage forKey:(NSString*)sessionID {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (greyImage)
    [greyImageDictionary_ setObject:greyImage forKey:sessionID];
  if ([sessionID isEqualToString:mostRecentGreySessionId_]) {
    mostRecentGreyBlock_.get()(greyImage);
    [self clearGreySessionInfo];
  }
}

- (void)loadGreyImageAsync:(NSString*)sessionID {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  // Don't call -retrieveImageForSessionID here because it caches the colored
  // image, which we don't need for the grey image cache. But if the image is
  // already in the cache, use it.
  UIImage* img = nil;
  if (lruCache_)
    img = [lruCache_ objectForKey:sessionID];
  else
    img = [imageDictionary_ objectForKey:sessionID];

  base::PostTaskAndReplyWithResult(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE_USER_BLOCKING)
          .get(),
      FROM_HERE, base::BindBlock(^base::scoped_nsobject<UIImage>() {
        base::scoped_nsobject<UIImage> result([img retain]);
        // If the image is not in the cache, load it from disk.
        if (!result)
          result.reset([ReadImageFromDisk(
              [SnapshotCache imagePathForSessionID:sessionID]) retain]);
        if (result)
          result.reset([GreyImage(result) retain]);
        return result;
      }),
      base::BindBlock(^(base::scoped_nsobject<UIImage> greyImage) {
        [self saveGreyImage:greyImage forKey:sessionID];
      }));
}

- (void)createGreyCache:(NSArray*)sessionIDs {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  greyImageDictionary_.reset(
      [[NSMutableDictionary alloc] initWithCapacity:kGreyInitialCapacity]);
  for (NSString* sessionID in sessionIDs)
    [self loadGreyImageAsync:sessionID];
}

- (void)removeGreyCache {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  greyImageDictionary_.reset();
  [self clearGreySessionInfo];
}

- (void)clearGreySessionInfo {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  mostRecentGreySessionId_.reset();
  mostRecentGreyBlock_.reset();
}

- (void)greyImageForSessionID:(NSString*)sessionID
                     callback:(void (^)(UIImage*))callback {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(greyImageDictionary_);
  UIImage* image = [greyImageDictionary_ objectForKey:sessionID];
  if (image) {
    callback(image);
    [self clearGreySessionInfo];
  } else {
    mostRecentGreySessionId_.reset([sessionID copy]);
    mostRecentGreyBlock_.reset([callback copy]);
  }
}

- (void)retrieveGreyImageForSessionID:(NSString*)sessionID
                             callback:(void (^)(UIImage*))callback {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (greyImageDictionary_) {
    UIImage* image = [greyImageDictionary_ objectForKey:sessionID];
    if (image) {
      callback(image);
      return;
    }
  }

  base::PostTaskAndReplyWithResult(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE_USER_BLOCKING)
          .get(),
      FROM_HERE, base::BindBlock(^base::scoped_nsobject<UIImage>() {
        // Retrieve the image on a high priority thread.
        // Loading the file into NSData is more reliable.
        // -imageWithContentsOfFile would ocassionally claim the image was not a
        // valid jpg.
        // "ImageIO: <ERROR> JPEGNot a JPEG file: starts with 0xff 0xd9"
        // See
        // http://stackoverflow.com/questions/5081297/ios-uiimagejpegrepresentation-error-not-a-jpeg-file-starts-with-0xff-0xd9
        NSData* imageData = [NSData
            dataWithContentsOfFile:base::SysUTF8ToNSString(
                [SnapshotCache greyImagePathForSessionID:sessionID].value())];
        if (!imageData)
          return base::scoped_nsobject<UIImage>();
        DCHECK(callback);
        return base::scoped_nsobject<UIImage>(
            [[UIImage imageWithData:imageData] retain]);
      }),
      base::BindBlock(^(base::scoped_nsobject<UIImage> image) {
        if (!image) {
          [self retrieveImageForSessionID:sessionID
                                 callback:^(UIImage* img) {
                                   if (callback && img)
                                     callback(GreyImage(img));
                                 }];
        } else if (callback) {
          callback(image);
        }
      }));
}

- (void)saveGreyInBackgroundForSessionID:(NSString*)sessionID {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (!sessionID)
    return;

  base::FilePath greyImagePath =
      GreyFilePathForSessionID(sessionID, [SnapshotCache cacheDirectory]);
  base::FilePath colorImagePath =
      FilePathForSessionID(sessionID, [SnapshotCache cacheDirectory]);

  // The color image may still be in memory.  Verify the sessionID matches.
  if (backgroundingColorImage_) {
    if (![backgroundingImageSessionId_ isEqualToString:sessionID]) {
      backgroundingColorImage_.reset();
      backgroundingImageSessionId_.reset();
    }
  }

  web::WebThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(&ConvertAndSaveGreyImage, colorImagePath,
                            greyImagePath, backgroundingColorImage_));
}

@end

@implementation SnapshotCache (TestingAdditions)

- (BOOL)hasImageInMemory:(NSString*)sessionID {
  if ([self usesLRUCache])
    return [lruCache_ objectForKey:sessionID] != nil;
  else
    return [imageDictionary_ objectForKey:sessionID] != nil;
}

- (BOOL)hasGreyImageInMemory:(NSString*)sessionID {
  return [greyImageDictionary_ objectForKey:sessionID] != nil;
}

- (NSUInteger)lruCacheMaxSize {
  return [lruCache_ maxCacheSize];
}

@end
