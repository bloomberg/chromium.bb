// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#import <Foundation/Foundation.h>
#import <ImageCaptureCore/ImageCaptureCore.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/system_monitor/image_capture_device.h"
#include "chrome/browser/system_monitor/image_capture_device_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

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

// Private ICCameraDevice method needed to properly initialize the object.
@interface NSObject (PrivateAPIICCameraDevice)
- (id)initWithDictionary:(id)properties;
@end

@interface MockICCameraDevice : ICCameraDevice {
 @private
  scoped_nsobject<NSMutableArray> allMediaFiles_;
}

- (void)addMediaFile:(ICCameraFile*)file;

@end

@implementation MockICCameraDevice

- (id)init {
  if ((self = [super initWithDictionary:[NSDictionary dictionary]])) {
  }
  return self;
}

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

// This method does approximately what the internal ImageCapture platform
// library is observed to do: take the download save-as filename and mangle
// it to attach an extension, then return that new filename to the caller
// in the options.
- (void)requestDownloadFile:(ICCameraFile*)file
                    options:(NSDictionary*)options
           downloadDelegate:(id<ICCameraDeviceDownloadDelegate>)downloadDelegate
        didDownloadSelector:(SEL)selector
                contextInfo:(void*)contextInfo {
  FilePath saveDir(base::SysNSStringToUTF8(
      [[options objectForKey:ICDownloadsDirectoryURL] path]));
  std::string saveAsFilename =
      base::SysNSStringToUTF8([options objectForKey:ICSaveAsFilename]);
  // It appears that the ImageCapture library adds an extension to the requested
  // filename. Do that here to require a rename.
  saveAsFilename += ".jpg";
  FilePath toBeSaved = saveDir.Append(saveAsFilename);
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

@interface MockICCameraFile : ICCameraFile {
 @private
  scoped_nsobject<NSString> name_;
  scoped_nsobject<NSDate> date_;
}

- (id)init:(NSString*)name;

@end

@implementation MockICCameraFile

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

class TestCameraListener
    : public ImageCaptureDeviceListener,
      public base::SupportsWeakPtr<TestCameraListener> {
 public:
  TestCameraListener()
      : completed_(false),
        removed_(false),
        last_error_(base::PLATFORM_FILE_ERROR_INVALID_URL) {}
  virtual ~TestCameraListener() {}

  virtual void ItemAdded(const std::string& name,
                         const base::PlatformFileInfo& info) OVERRIDE {
    items_.push_back(name);
  }

  virtual void NoMoreItems() OVERRIDE {
    completed_ = true;
  }

  virtual void DownloadedFile(const std::string& name,
                              base::PlatformFileError error) OVERRIDE {
    EXPECT_TRUE(content::BrowserThread::CurrentlyOn(
        content::BrowserThread::UI));
    downloads_.push_back(name);
    last_error_ = error;
  }

  virtual void DeviceRemoved() OVERRIDE {
    removed_ = true;
  }

  std::vector<std::string> items() const { return items_; }
  std::vector<std::string> downloads() const { return downloads_; }
  bool completed() const { return completed_; }
  bool removed() const { return removed_; }
  base::PlatformFileError last_error() const { return last_error_; }

 private:
  std::vector<std::string> items_;
  std::vector<std::string> downloads_;
  bool completed_;
  bool removed_;
  base::PlatformFileError last_error_;
};

class ImageCaptureDeviceManagerTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    base::SystemMonitor::AllocateSystemIOPorts();
    system_monitor_.reset(new base::SystemMonitor());
    ui_thread_.reset(new content::TestBrowserThread(
        content::BrowserThread::UI, &message_loop_));
  }

  MockICCameraDevice* AttachDevice(
      chrome::ImageCaptureDeviceManager* manager) {
    // Ownership will be passed to the device browser delegate.
    scoped_nsobject<MockICCameraDevice> device(
        [[MockICCameraDevice alloc] init]);
    id<ICDeviceBrowserDelegate> delegate = manager->device_browser();
    [delegate deviceBrowser:nil didAddDevice:device moreComing:NO];
    return device.autorelease();
  }

  void DetachDevice(chrome::ImageCaptureDeviceManager* manager,
                    ICCameraDevice* device) {
    id<ICDeviceBrowserDelegate> delegate = manager->device_browser();
    [delegate deviceBrowser:nil didRemoveDevice:device moreGoing:NO];
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  scoped_ptr<base::SystemMonitor> system_monitor_;
  TestCameraListener listener_;
};

TEST_F(ImageCaptureDeviceManagerTest, TestAttachDetach) {
  chrome::ImageCaptureDeviceManager manager;
  ICCameraDevice* device = AttachDevice(&manager);
  std::vector<base::SystemMonitor::RemovableStorageInfo> devices =
    system_monitor_->GetAttachedRemovableStorage();

  ASSERT_EQ(1U, devices.size());
  EXPECT_EQ(std::string("ic:") + kDeviceId, devices[0].device_id);

  DetachDevice(&manager, device);
  devices = system_monitor_->GetAttachedRemovableStorage();
  ASSERT_EQ(0U, devices.size());
};

TEST_F(ImageCaptureDeviceManagerTest, OpenCamera) {
  chrome::ImageCaptureDeviceManager manager;
  ICCameraDevice* device = AttachDevice(&manager);

  EXPECT_FALSE(chrome::ImageCaptureDeviceManager::deviceForUUID(
      "nonexistent"));

  scoped_nsobject<ImageCaptureDevice> camera(
      [chrome::ImageCaptureDeviceManager::deviceForUUID(kDeviceId)
          retain]);

  [camera setListener:listener_.AsWeakPtr()];
  [camera open];

  scoped_nsobject<MockICCameraFile> picture1(
      [[MockICCameraFile alloc] init:@"pic1"]);
  [camera cameraDevice:nil didAddItem:picture1];
  scoped_nsobject<MockICCameraFile> picture2(
      [[MockICCameraFile alloc] init:@"pic2"]);
  [camera cameraDevice:nil didAddItem:picture2];
  ASSERT_EQ(2U, listener_.items().size());
  EXPECT_EQ("pic1", listener_.items()[0]);
  EXPECT_EQ("pic2", listener_.items()[1]);
  EXPECT_FALSE(listener_.completed());

  [camera deviceDidBecomeReadyWithCompleteContentCatalog:nil];

  ASSERT_EQ(2U, listener_.items().size());
  EXPECT_TRUE(listener_.completed());

  [camera close];
  DetachDevice(&manager, device);
  EXPECT_FALSE(chrome::ImageCaptureDeviceManager::deviceForUUID(
      kDeviceId));
}

TEST_F(ImageCaptureDeviceManagerTest, RemoveCamera) {
  chrome::ImageCaptureDeviceManager manager;
  ICCameraDevice* device = AttachDevice(&manager);

  scoped_nsobject<ImageCaptureDevice> camera(
      [chrome::ImageCaptureDeviceManager::deviceForUUID(kDeviceId)
          retain]);

  [camera setListener:listener_.AsWeakPtr()];
  [camera open];

  [camera didRemoveDevice:device];
  EXPECT_TRUE(listener_.removed());
}

TEST_F(ImageCaptureDeviceManagerTest, DownloadFile) {
  scoped_ptr<content::TestBrowserThread> file_thread_(
      new content::TestBrowserThread(
          content::BrowserThread::FILE, &message_loop_));

  chrome::ImageCaptureDeviceManager manager;
  MockICCameraDevice* device = AttachDevice(&manager);

  scoped_nsobject<ImageCaptureDevice> camera(
      [chrome::ImageCaptureDeviceManager::deviceForUUID(kDeviceId)
          retain]);

  [camera setListener:listener_.AsWeakPtr()];
  [camera open];

  std::string kTestFileName("pic1");

  scoped_nsobject<MockICCameraFile> picture1(
      [[MockICCameraFile alloc]
          init:base::SysUTF8ToNSString(kTestFileName)]);
  [device addMediaFile:picture1];
  [camera cameraDevice:nil didAddItem:picture1];

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EXPECT_EQ(0U, listener_.downloads().size());

  // Test that a nonexistent file we ask to be downloaded will
  // return us a not-found error.
  FilePath temp_file = temp_dir.path().Append("tempfile");
  [camera downloadFile:std::string("nonexistent") localPath:temp_file];
  message_loop_.RunUntilIdle();
  ASSERT_EQ(1U, listener_.downloads().size());
  EXPECT_EQ("nonexistent", listener_.downloads()[0]);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, listener_.last_error());

  // Test that an existing file we ask to be downloaded will end up in
  // the location we specify. The mock system will copy testing file
  // contents to a separate filename, mimicking the ImageCaptureCore
  // library behavior. Our code then renames the file onto the requested
  // destination.
  [camera downloadFile:kTestFileName localPath:temp_file];
  message_loop_.RunUntilIdle();

  ASSERT_EQ(2U, listener_.downloads().size());
  EXPECT_EQ(kTestFileName, listener_.downloads()[1]);
  ASSERT_EQ(base::PLATFORM_FILE_OK, listener_.last_error());
  char file_contents[5];
  ASSERT_EQ(4, file_util::ReadFile(temp_file, file_contents,
                                   strlen(kTestFileContents)));
  EXPECT_EQ(kTestFileContents,
            std::string(file_contents, strlen(kTestFileContents)));

  [camera didRemoveDevice:device];
}
