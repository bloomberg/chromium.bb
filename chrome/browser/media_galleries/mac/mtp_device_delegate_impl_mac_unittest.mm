// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <ImageCaptureCore/ImageCaptureCore.h>

#include "base/file_util.h"
#include "base/mac/cocoa_protocols.h"
#include "base/mac/foundation_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_string_conversions.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media_galleries/mac/mtp_device_delegate_impl_mac.h"
#include "chrome/browser/storage_monitor/image_capture_device_manager.h"
#include "chrome/browser/storage_monitor/test_storage_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_file_util.h"

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface NSObject (ICCameraDeviceDelegateLionAPI)
- (void)deviceDidBecomeReadyWithCompleteContentCatalog:(ICDevice*)device;
- (void)didDownloadFile:(ICCameraFile*)file
                  error:(NSError*)error
                options:(NSDictionary*)options
            contextInfo:(void*)contextInfo;
@end

#endif  // 10.6

namespace {

const char kDeviceId[] = "id";
const char kTestFileContents[] = "test";

}  // namespace

@interface MockMTPICCameraDevice : ICCameraDevice {
 @private
  scoped_nsobject<NSMutableArray> allMediaFiles_;
}

- (void)addMediaFile:(ICCameraFile*)file;

@end

@implementation MockMTPICCameraDevice

- (NSString*)mountPoint {
  return @"mountPoint";
}

- (NSString*)name {
  return @"name";
}

- (NSString*)UUIDString {
  return base::SysUTF8ToNSString(kDeviceId);
}

- (ICDeviceType)type {
  return ICDeviceTypeCamera;
}

- (void)requestOpenSession {
}

- (void)requestCloseSession {
}

- (NSArray*)mediaFiles {
  return allMediaFiles_;
}

- (void)addMediaFile:(ICCameraFile*)file {
  if (!allMediaFiles_.get())
    allMediaFiles_.reset([[NSMutableArray alloc] init]);
  [allMediaFiles_ addObject:file];
}

- (void)requestDownloadFile:(ICCameraFile*)file
                    options:(NSDictionary*)options
           downloadDelegate:(id<ICCameraDeviceDownloadDelegate>)downloadDelegate
        didDownloadSelector:(SEL)selector
                contextInfo:(void*)contextInfo {
  base::FilePath saveDir(base::SysNSStringToUTF8(
      [[options objectForKey:ICDownloadsDirectoryURL] path]));
  std::string saveAsFilename =
      base::SysNSStringToUTF8([options objectForKey:ICSaveAsFilename]);
  // It appears that the ImageCapture library adds an extension to the requested
  // filename. Do that here to require a rename.
  saveAsFilename += ".jpg";
  base::FilePath toBeSaved = saveDir.Append(saveAsFilename);
  ASSERT_EQ(static_cast<int>(strlen(kTestFileContents)),
            file_util::WriteFile(toBeSaved, kTestFileContents,
                                 strlen(kTestFileContents)));

  NSMutableDictionary* returnOptions =
      [NSMutableDictionary dictionaryWithDictionary:options];
  [returnOptions setObject:base::SysUTF8ToNSString(saveAsFilename)
                    forKey:ICSavedFilename];

  [static_cast<NSObject<ICCameraDeviceDownloadDelegate>*>(downloadDelegate)
   didDownloadFile:file
             error:nil
           options:returnOptions
       contextInfo:contextInfo];
}

@end

@interface MockMTPICCameraFile : ICCameraFile {
 @private
  scoped_nsobject<NSString> name_;
  scoped_nsobject<NSDate> date_;
}

- (id)init:(NSString*)name;

@end

@implementation MockMTPICCameraFile

- (id)init:(NSString*)name {
  if ((self = [super init])) {
    name_.reset([name retain]);
    date_.reset([[NSDate dateWithNaturalLanguageString:@"12/12/12"] retain]);
  }
  return self;
}

- (NSString*)name {
  return name_.get();
}

- (NSString*)UTI {
  return base::mac::CFToNSCast(kUTTypeImage);
}

- (NSDate*)modificationDate {
  return date_.get();
}

- (NSDate*)creationDate {
  return date_.get();
}

- (off_t)fileSize {
  return 1000;
}

@end

// Advances the enumerator. When the method returns, signals the waiting
// event.
void EnumerateAndSignal(
    fileapi::FileSystemFileUtil::AbstractFileEnumerator* enumerator,
    base::WaitableEvent* event,
    base::FilePath* path) {
  *path = enumerator->Next();
  event->Signal();
}

class MTPDeviceDelegateImplMacTest : public testing::Test {
 public:
  MTPDeviceDelegateImplMacTest() : camera_(NULL), delegate_(NULL) {}

  virtual void SetUp() OVERRIDE {
    ui_thread_.reset(new content::TestBrowserThread(
        content::BrowserThread::UI, &message_loop_));

    manager_.SetNotifications(monitor_.receiver());

    camera_ = [MockMTPICCameraDevice alloc];
    id<ICDeviceBrowserDelegate> delegate = manager_.device_browser();
    [delegate deviceBrowser:nil didAddDevice:camera_ moreComing:NO];

    base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
    task_runner_ = pool->GetSequencedTaskRunner(
        pool->GetNamedSequenceToken("token-name"));
    delegate_ = new chrome::MTPDeviceDelegateImplMac(
        "id", "/ic:id", task_runner_.get());
  }

  virtual void TearDown() OVERRIDE {
    id<ICDeviceBrowserDelegate> delegate = manager_.device_browser();
    [delegate deviceBrowser:nil didRemoveDevice:camera_ moreGoing:NO];

    task_runner_->PostTask(FROM_HERE,
        base::Bind(&chrome::MTPDeviceDelegateImplMac::
                        CancelPendingTasksAndDeleteDelegate,
                   base::Unretained(delegate_)));
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  chrome::test::TestStorageMonitor monitor_;
  chrome::ImageCaptureDeviceManager manager_;
  ICCameraDevice* camera_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // This object needs special deletion inside the above |task_runner_|.
  chrome::MTPDeviceDelegateImplMac* delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MTPDeviceDelegateImplMacTest);
};

TEST_F(MTPDeviceDelegateImplMacTest, TestGetRootFileInfo) {
  base::PlatformFileInfo info;
  // Making a fresh delegate should have a single file entry for the synthetic
  // root directory, with the name equal to the device id string.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            delegate_->GetFileInfo(base::FilePath("/ic:id"), &info));
  EXPECT_TRUE(info.is_directory);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            delegate_->GetFileInfo(base::FilePath("/nonexistent"), &info));

  // Signal the delegate that no files are coming.
  delegate_->NoMoreItems();

  scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator> enumerator =
      delegate_->CreateFileEnumerator(base::FilePath("/ic:id"), true);
  EXPECT_TRUE(enumerator->Next().empty());
}

TEST_F(MTPDeviceDelegateImplMacTest, TestGetFileInfo) {
  base::Time time1 = base::Time::Now();
  base::PlatformFileInfo info1;
  info1.size = 1;
  info1.is_directory = false;
  info1.is_symbolic_link = false;
  info1.last_modified = time1;
  info1.last_accessed = time1;
  info1.creation_time = time1;
  delegate_->ItemAdded("name1", info1);

  base::PlatformFileInfo info;
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            delegate_->GetFileInfo(base::FilePath("/ic:id/name1"), &info));
  EXPECT_EQ(info1.size, info.size);
  EXPECT_EQ(info1.is_directory, info.is_directory);
  EXPECT_EQ(info1.last_modified, info.last_modified);
  EXPECT_EQ(info1.last_accessed, info.last_accessed);
  EXPECT_EQ(info1.creation_time, info.creation_time);

  info1.size = 2;
  delegate_->ItemAdded("name2", info1);
  delegate_->NoMoreItems();

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            delegate_->GetFileInfo(base::FilePath("/ic:id/name2"), &info));
  EXPECT_EQ(info1.size, info.size);

  scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator> enumerator =
      delegate_->CreateFileEnumerator(base::FilePath("/ic:id"), true);
  base::FilePath next = enumerator->Next();
  ASSERT_FALSE(next.empty());
  EXPECT_EQ(1, enumerator->Size());
  EXPECT_EQ(time1, enumerator->LastModifiedTime());
  EXPECT_FALSE(enumerator->IsDirectory());
  EXPECT_EQ("/ic:id/name1", next.value());

  next = enumerator->Next();
  ASSERT_FALSE(next.empty());
  EXPECT_EQ(2, enumerator->Size());
  EXPECT_EQ(time1, enumerator->LastModifiedTime());
  EXPECT_FALSE(enumerator->IsDirectory());
  EXPECT_EQ("/ic:id/name2", next.value());

  next = enumerator->Next();
  EXPECT_TRUE(next.empty());
}

TEST_F(MTPDeviceDelegateImplMacTest, TestIgnoreDirectories) {
  base::Time time1 = base::Time::Now();
  base::PlatformFileInfo info1;
  info1.size = 1;
  info1.is_directory = false;
  info1.is_symbolic_link = false;
  info1.last_modified = time1;
  info1.last_accessed = time1;
  info1.creation_time = time1;
  delegate_->ItemAdded("name1", info1);

  info1.is_directory = true;
  delegate_->ItemAdded("dir1", info1);
  delegate_->ItemAdded("dir2", info1);

  info1.is_directory = false;
  delegate_->ItemAdded("name2", info1);
  delegate_->NoMoreItems();

  scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator> enumerator =
      delegate_->CreateFileEnumerator(base::FilePath("/ic:id"), true);
  base::FilePath next = enumerator->Next();
  ASSERT_FALSE(next.empty());
  EXPECT_EQ("/ic:id/name1", next.value());

  next = enumerator->Next();
  ASSERT_FALSE(next.empty());
  EXPECT_EQ("/ic:id/name2", next.value());

  next = enumerator->Next();
  EXPECT_TRUE(next.empty());
}

TEST_F(MTPDeviceDelegateImplMacTest, EnumeratorWaitsForEntries) {
  base::Time time1 = base::Time::Now();
  base::PlatformFileInfo info1;
  info1.size = 1;
  info1.is_directory = false;
  info1.is_symbolic_link = false;
  info1.last_modified = time1;
  info1.last_accessed = time1;
  info1.creation_time = time1;
  delegate_->ItemAdded("name1", info1);

  scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator> enumerator =
      delegate_->CreateFileEnumerator(base::FilePath("/ic:id"), true);
  // Event is manually reset, initially unsignaled
  base::WaitableEvent event(true, false);
  base::FilePath next;
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&EnumerateAndSignal,
                         enumerator.get(), &event, &next));
  message_loop_.RunUntilIdle();
  ASSERT_TRUE(event.IsSignaled());
  EXPECT_EQ("/ic:id/name1", next.value());

  event.Reset();

  // This method will block until it is sure there are no more items.
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&EnumerateAndSignal,
                         enumerator.get(), &event, &next));
  message_loop_.RunUntilIdle();
  ASSERT_FALSE(event.IsSignaled());
  delegate_->NoMoreItems();
  event.Wait();
  ASSERT_TRUE(event.IsSignaled());
  EXPECT_TRUE(next.empty());
  message_loop_.RunUntilIdle();
}
