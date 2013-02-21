// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/storage_monitor/image_capture_device.h"

#include "base/file_util.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "content/public/browser/browser_thread.h"

namespace {

void RenameFile(const base::FilePath& downloaded_filename,
                const base::FilePath& desired_filename,
                base::PlatformFileError* result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  bool success = file_util::ReplaceFile(downloaded_filename, desired_filename);
  *result = success ? base::PLATFORM_FILE_OK
                    : base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

void ReturnRenameResultToListener(
    base::WeakPtr<ImageCaptureDeviceListener> listener,
    const std::string& name,
    base::PlatformFileError* result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  scoped_ptr<base::PlatformFileError> result_deleter(result);
  if (listener)
    listener->DownloadedFile(name, *result);
}

base::Time NSDateToBaseTime(NSDate* date) {
  return base::Time::FromDoubleT([date timeIntervalSince1970]);
}

}  // namespace

@implementation ImageCaptureDevice

- (id)initWithCameraDevice:(ICCameraDevice*)cameraDevice {
  if ((self = [super init])) {
    camera_.reset([cameraDevice retain]);
    [camera_ setDelegate:self];
  }
  return self;
}

- (void)dealloc {
  // Make sure the session was closed and listener set to null
  // before destruction.
  DCHECK(![camera_ delegate]);
  DCHECK(!listener_);
  [super dealloc];
}

- (void)setListener:(base::WeakPtr<ImageCaptureDeviceListener>)listener {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  listener_ = listener;
}

- (void)open {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(listener_);
  [camera_ requestOpenSession];
}

- (void)close {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  [camera_ requestCloseSession];
  [camera_ setDelegate:nil];
  listener_.reset();
}

- (void)downloadFile:(const std::string&)name
           localPath:(const base::FilePath&)localPath {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Find the file with that name and start download.
  for (ICCameraItem* item in [camera_ mediaFiles]) {
    std::string itemName = base::SysNSStringToUTF8([item name]);
    if (itemName == name) {
      // To create save options for ImageCapture, we need to
      // split the target filename into directory/name
      // and encode the directory as a URL.
      NSString* saveDirectory =
          base::mac::FilePathToNSString(localPath.DirName());
      NSString* saveFilename =
          base::mac::FilePathToNSString(localPath.BaseName());

      NSMutableDictionary* options =
          [NSMutableDictionary dictionaryWithCapacity:3];
      [options setObject:[NSURL fileURLWithPath:saveDirectory isDirectory:YES]
                  forKey:ICDownloadsDirectoryURL];
      [options setObject:saveFilename forKey:ICSaveAsFilename];
      [options setObject:[NSNumber numberWithBool:YES] forKey:ICOverwrite];

      [camera_ requestDownloadFile:base::mac::ObjCCastStrict<ICCameraFile>(item)
                           options:options
                  downloadDelegate:self
               didDownloadSelector:
                   @selector(didDownloadFile:error:options:contextInfo:)
                       contextInfo:NULL];
      return;
    }
  }

  if (listener_)
    listener_->DownloadedFile(name, base::PLATFORM_FILE_ERROR_NOT_FOUND);
}

- (void)cameraDevice:(ICCameraDevice*)camera didAddItem:(ICCameraItem*)item {
  std::string name = base::SysNSStringToUTF8([item name]);
  base::PlatformFileInfo info;
  if ([[item UTI] isEqualToString:base::mac::CFToNSCast(kUTTypeFolder)])
    info.is_directory = true;
  else
    info.size = [base::mac::ObjCCastStrict<ICCameraFile>(item) fileSize];
  info.last_modified = NSDateToBaseTime([item modificationDate]);
  info.creation_time = NSDateToBaseTime([item creationDate]);
  info.last_accessed = info.last_modified;

  if (listener_)
    listener_->ItemAdded(name, info);
}

- (void)cameraDevice:(ICCameraDevice*)camera didAddItems:(NSArray*)items {
  for (ICCameraItem* item in items)
    [self cameraDevice:camera didAddItem:item];
}

- (void)didRemoveDevice:(ICDevice*)device {
  device.delegate = NULL;
  if (listener_)
    listener_->DeviceRemoved();
}

// Notifies that a session was opened with the given device; potentially
// with an error.
- (void)device:(ICDevice*)device didOpenSessionWithError:(NSError*)error {
  if (error)
    [self didRemoveDevice:camera_];
}

- (void)device:(ICDevice*)device didEncounterError:(NSError*)error {
  if (error && listener_)
    listener_->DeviceRemoved();
}

// When this message is received, all media metadata is now loaded.
- (void)deviceDidBecomeReadyWithCompleteContentCatalog:(ICDevice*)device {
  if (listener_)
    listener_->NoMoreItems();
}

- (void)didDownloadFile:(ICCameraFile*)file
                  error:(NSError*)error
                options:(NSDictionary*)options
            contextInfo:(void*)contextInfo {
  std::string name = base::SysNSStringToUTF8([file name]);

  if (error) {
    DLOG(INFO) << "error..."
               << base::SysNSStringToUTF8([error localizedDescription]);
    if (listener_)
      listener_->DownloadedFile(name, base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }

  std::string savedFilename =
      base::SysNSStringToUTF8([options objectForKey:ICSavedFilename]);
  std::string saveAsFilename =
      base::SysNSStringToUTF8([options objectForKey:ICSaveAsFilename]);
  if (savedFilename == saveAsFilename) {
    if (listener_)
      listener_->DownloadedFile(name, base::PLATFORM_FILE_OK);
    return;
  }

  // ImageCapture did not save the file into the name we gave it in the
  // options. It picks a new name according to its best lights, so we need
  // to rename the file.
  base::FilePath saveDir(base::SysNSStringToUTF8(
      [[options objectForKey:ICDownloadsDirectoryURL] path]));
  base::FilePath saveAsPath = saveDir.Append(saveAsFilename);
  base::FilePath savedPath = saveDir.Append(savedFilename);
  // Shared result value from file-copy closure to tell-listener closure.
  base::PlatformFileError* copyResult = new base::PlatformFileError();
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&RenameFile, savedPath, saveAsPath, copyResult),
      base::Bind(&ReturnRenameResultToListener, listener_, name, copyResult));
}

@end  // ImageCaptureDevice
