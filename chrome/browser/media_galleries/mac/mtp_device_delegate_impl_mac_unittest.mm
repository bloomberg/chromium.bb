// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <ImageCaptureCore/ImageCaptureCore.h>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/cocoa_protocols.h"
#include "base/mac/foundation_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/strings/sys_string_conversions.h"
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
const char kDevicePath[] = "/ic:id";
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
    file_thread_.reset(new content::TestBrowserThread(
        content::BrowserThread::FILE, &message_loop_));
    io_thread_.reset(new content::TestBrowserThread(
        content::BrowserThread::IO));
    ASSERT_TRUE(io_thread_->Start());

    manager_.SetNotifications(monitor_.receiver());

    camera_ = [MockMTPICCameraDevice alloc];
    id<ICDeviceBrowserDelegate> delegate = manager_.device_browser();
    [delegate deviceBrowser:nil didAddDevice:camera_ moreComing:NO];

    delegate_ = new chrome::MTPDeviceDelegateImplMac(kDeviceId, kDevicePath);
  }

  void OnError(base::WaitableEvent* event, base::PlatformFileError error) {
    error_ = error;
    event->Signal();
  }

  void OverlappedOnError(base::WaitableEvent* event,
                         base::PlatformFileError error) {
    overlapped_error_ = error;
    event->Signal();
  }

  void OnFileInfo(base::WaitableEvent* event,
                  const base::PlatformFileInfo& info) {
    error_ = base::PLATFORM_FILE_OK;
    info_ = info;
    event->Signal();
  }

  void OnReadDir(base::WaitableEvent* event,
                 const fileapi::AsyncFileUtil::EntryList& files,
                 bool has_more) {
    error_ = base::PLATFORM_FILE_OK;
    ASSERT_FALSE(has_more);
    file_list_ = files;
    event->Signal();
  }

  void OverlappedOnReadDir(base::WaitableEvent* event,
                           const fileapi::AsyncFileUtil::EntryList& files,
                           bool has_more) {
    overlapped_error_ = base::PLATFORM_FILE_OK;
    ASSERT_FALSE(has_more);
    overlapped_file_list_ = files;
    event->Signal();
  }

  void OnDownload(base::WaitableEvent* event,
                  const base::PlatformFileInfo& file_info,
                  const base::FilePath& local_path) {
    error_ = base::PLATFORM_FILE_OK;
    event->Signal();
  }

  base::PlatformFileError GetFileInfo(const base::FilePath& path,
                                      base::PlatformFileInfo* info) {
    base::WaitableEvent wait(true, false);
    delegate_->GetFileInfo(
      path,
      base::Bind(&MTPDeviceDelegateImplMacTest::OnFileInfo,
                 base::Unretained(this),
                 &wait),
      base::Bind(&MTPDeviceDelegateImplMacTest::OnError,
                 base::Unretained(this),
                 &wait));
    base::RunLoop loop;
    loop.RunUntilIdle();
    EXPECT_TRUE(wait.IsSignaled());
    *info = info_;
    return error_;
  }

  base::PlatformFileError ReadDir(const base::FilePath& path) {
    base::WaitableEvent wait(true, false);
    delegate_->ReadDirectory(
        path,
        base::Bind(&MTPDeviceDelegateImplMacTest::OnReadDir,
                   base::Unretained(this),
                   &wait),
        base::Bind(&MTPDeviceDelegateImplMacTest::OnError,
                   base::Unretained(this),
                   &wait));
    base::RunLoop loop;
    loop.RunUntilIdle();
    wait.Wait();
    return error_;
  }

  base::PlatformFileError DownloadFile(
      const base::FilePath& path,
      const base::FilePath& local_path) {
    base::WaitableEvent wait(true, false);
    delegate_->CreateSnapshotFile(
        path, local_path,
        base::Bind(&MTPDeviceDelegateImplMacTest::OnDownload,
                   base::Unretained(this),
                   &wait),
        base::Bind(&MTPDeviceDelegateImplMacTest::OnError,
                   base::Unretained(this),
                   &wait));
    base::RunLoop loop;
    loop.RunUntilIdle();
    wait.Wait();
    return error_;
  }

  virtual void TearDown() OVERRIDE {
    id<ICDeviceBrowserDelegate> delegate = manager_.device_browser();
    [delegate deviceBrowser:nil didRemoveDevice:camera_ moreGoing:NO];

    delegate_->CancelPendingTasksAndDeleteDelegate();

    io_thread_->Stop();
  }

 protected:
  MessageLoopForUI message_loop_;
  // Note: threads must be made in this order: UI > FILE > IO
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  scoped_ptr<content::TestBrowserThread> file_thread_;
  scoped_ptr<content::TestBrowserThread> io_thread_;
  base::ScopedTempDir temp_dir_;
  chrome::test::TestStorageMonitor monitor_;
  chrome::ImageCaptureDeviceManager manager_;
  MockMTPICCameraDevice* camera_;

  // This object needs special deletion inside the above |task_runner_|.
  chrome::MTPDeviceDelegateImplMac* delegate_;

  base::PlatformFileError error_;
  base::PlatformFileInfo info_;
  fileapi::AsyncFileUtil::EntryList file_list_;

  base::PlatformFileError overlapped_error_;
  fileapi::AsyncFileUtil::EntryList overlapped_file_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MTPDeviceDelegateImplMacTest);
};

TEST_F(MTPDeviceDelegateImplMacTest, TestGetRootFileInfo) {
  base::PlatformFileInfo info;
  // Making a fresh delegate should have a single file entry for the synthetic
  // root directory, with the name equal to the device id string.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            GetFileInfo(base::FilePath(kDevicePath), &info));
  EXPECT_TRUE(info.is_directory);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            GetFileInfo(base::FilePath("/nonexistent"), &info));

  // Signal the delegate that no files are coming.
  delegate_->NoMoreItems();

  EXPECT_EQ(base::PLATFORM_FILE_OK, ReadDir(base::FilePath(kDevicePath)));
  EXPECT_EQ(0U, file_list_.size());
}

TEST_F(MTPDeviceDelegateImplMacTest, TestOverlappedReadDir) {
  base::Time time1 = base::Time::Now();
  base::PlatformFileInfo info1;
  info1.size = 1;
  info1.is_directory = false;
  info1.is_symbolic_link = false;
  info1.last_modified = time1;
  info1.last_accessed = time1;
  info1.creation_time = time1;
  delegate_->ItemAdded("name1", info1);

  base::WaitableEvent wait(true, false);

  delegate_->ReadDirectory(
      base::FilePath(kDevicePath),
      base::Bind(&MTPDeviceDelegateImplMacTest::OnReadDir,
                 base::Unretained(this),
                 &wait),
      base::Bind(&MTPDeviceDelegateImplMacTest::OnError,
                 base::Unretained(this),
                 &wait));

  delegate_->ReadDirectory(
      base::FilePath(kDevicePath),
      base::Bind(&MTPDeviceDelegateImplMacTest::OverlappedOnReadDir,
                 base::Unretained(this),
                 &wait),
      base::Bind(&MTPDeviceDelegateImplMacTest::OverlappedOnError,
                 base::Unretained(this),
                 &wait));


  // Signal the delegate that no files are coming.
  delegate_->NoMoreItems();

  base::RunLoop loop;
  loop.RunUntilIdle();
  wait.Wait();

  EXPECT_EQ(base::PLATFORM_FILE_OK, error_);
  EXPECT_EQ(1U, file_list_.size());
  EXPECT_EQ(base::PLATFORM_FILE_OK, overlapped_error_);
  EXPECT_EQ(1U, overlapped_file_list_.size());
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
            GetFileInfo(base::FilePath("/ic:id/name1"), &info));
  EXPECT_EQ(info1.size, info.size);
  EXPECT_EQ(info1.is_directory, info.is_directory);
  EXPECT_EQ(info1.last_modified, info.last_modified);
  EXPECT_EQ(info1.last_accessed, info.last_accessed);
  EXPECT_EQ(info1.creation_time, info.creation_time);

  info1.size = 2;
  delegate_->ItemAdded("name2", info1);
  delegate_->NoMoreItems();

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            GetFileInfo(base::FilePath("/ic:id/name2"), &info));
  EXPECT_EQ(info1.size, info.size);

  EXPECT_EQ(base::PLATFORM_FILE_OK, ReadDir(base::FilePath(kDevicePath)));

  ASSERT_EQ(2U, file_list_.size());
  EXPECT_EQ(time1, file_list_[0].last_modified_time);
  EXPECT_FALSE(file_list_[0].is_directory);
  EXPECT_EQ("/ic:id/name1", file_list_[0].name);

  EXPECT_EQ(time1, file_list_[1].last_modified_time);
  EXPECT_FALSE(file_list_[1].is_directory);
  EXPECT_EQ("/ic:id/name2", file_list_[1].name);
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

  EXPECT_EQ(base::PLATFORM_FILE_OK, ReadDir(base::FilePath(kDevicePath)));

  ASSERT_EQ(2U, file_list_.size());
  EXPECT_EQ(time1, file_list_[0].last_modified_time);
  EXPECT_FALSE(file_list_[0].is_directory);
  EXPECT_EQ("/ic:id/name1", file_list_[0].name);

  EXPECT_EQ(time1, file_list_[1].last_modified_time);
  EXPECT_FALSE(file_list_[1].is_directory);
  EXPECT_EQ("/ic:id/name2", file_list_[1].name);
}

TEST_F(MTPDeviceDelegateImplMacTest, TestDownload) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::Time t1 = base::Time::Now();
  base::PlatformFileInfo info;
  info.size = 4;
  info.is_directory = false;
  info.is_symbolic_link = false;
  info.last_modified = t1;
  info.last_accessed = t1;
  info.creation_time = t1;
  std::string kTestFileName("filename");
  scoped_nsobject<MockMTPICCameraFile> picture1(
      [[MockMTPICCameraFile alloc]
          init:base::SysUTF8ToNSString(kTestFileName)]);
  [camera_ addMediaFile:picture1];
  delegate_->ItemAdded(kTestFileName, info);
  delegate_->NoMoreItems();

  EXPECT_EQ(base::PLATFORM_FILE_OK, ReadDir(base::FilePath(kDevicePath)));
  ASSERT_EQ(1U, file_list_.size());
  ASSERT_EQ("/ic:id/filename", file_list_[0].name);

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            DownloadFile(base::FilePath("/ic:id/nonexist"),
                         temp_dir_.path().Append("target")));

  EXPECT_EQ(base::PLATFORM_FILE_OK,
             DownloadFile(base::FilePath("/ic:id/filename"),
                          temp_dir_.path().Append("target")));
  std::string contents;
  EXPECT_TRUE(file_util::ReadFileToString(temp_dir_.path().Append("target"),
                                          &contents));
  EXPECT_EQ(kTestFileContents, contents);
}
