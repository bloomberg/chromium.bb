// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_paths.h"
#import "ios/chrome/browser/sessions/session_service.h"
#import "ios/chrome/browser/sessions/session_window.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface SessionServiceIOS (Testing)
- (void)performSaveWindow:(SessionWindowIOS*)window
              toDirectory:(NSString*)directory;
@end

namespace {

// Fixture Class. Takes care of deleting the directory used to store test data.
class SessionServiceTest : public PlatformTest {
 private:
  base::ScopedTempDir test_dir_;

 protected:
  void SetUp() override {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    //    directoryName_ = [NSString
    //        stringWithCString:test_dir_.path().value().c_str()
    //                 encoding:NSASCIIStringEncoding];

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPath(test_dir_.GetPath());
    chrome_browser_state_ = test_cbs_builder.Build();
    directoryName_ =
        base::SysUTF8ToNSString(chrome_browser_state_->GetStatePath().value());
  }

  void TearDown() override {}

  // Helper function to load a SessionWindowIOS from a given testdata
  // |filename|.  Returns nil if there was an error loading the session.
  SessionWindowIOS* LoadSessionFromTestDataFile(
      const base::FilePath::StringType& filename) {
    SessionServiceIOS* service = [SessionServiceIOS sharedService];
    base::FilePath plist_path;
    bool success = PathService::Get(ios::DIR_TEST_DATA, &plist_path);
    EXPECT_TRUE(success);
    if (!success) {
      return nil;
    }

    plist_path = plist_path.AppendASCII("sessions");
    plist_path = plist_path.Append(filename);
    EXPECT_TRUE(base::PathExists(plist_path));

    NSString* path = base::SysUTF8ToNSString(plist_path.value());
    return [service loadWindowFromPath:path
                       forBrowserState:chrome_browser_state_.get()];
  }

  NSString* directoryName_;
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(SessionServiceTest, Singleton) {
  SessionServiceIOS* service = [SessionServiceIOS sharedService];
  EXPECT_TRUE(service != nil);

  SessionServiceIOS* anotherService = [SessionServiceIOS sharedService];
  EXPECT_TRUE(anotherService != nil);

  EXPECT_TRUE(service == anotherService);
}

TEST_F(SessionServiceTest, SaveWindowToDirectory) {
  id sessionWindowMock =
      [OCMockObject niceMockForClass:[SessionWindowIOS class]];
  SessionServiceIOS* service = [SessionServiceIOS sharedService];
  [service performSaveWindow:sessionWindowMock toDirectory:directoryName_];

  NSFileManager* fileManager = [NSFileManager defaultManager];
  EXPECT_TRUE([fileManager removeItemAtPath:directoryName_ error:NULL]);
}

TEST_F(SessionServiceTest, SaveWindowToDirectoryAlreadyExistent) {
  id sessionWindowMock =
      [OCMockObject niceMockForClass:[SessionWindowIOS class]];
  EXPECT_TRUE([[NSFileManager defaultManager]
            createDirectoryAtPath:directoryName_
      withIntermediateDirectories:YES
                       attributes:nil
                            error:NULL]);

  SessionServiceIOS* service = [SessionServiceIOS sharedService];
  [service performSaveWindow:sessionWindowMock toDirectory:directoryName_];

  NSFileManager* fileManager = [NSFileManager defaultManager];
  EXPECT_TRUE([fileManager removeItemAtPath:directoryName_ error:NULL]);
}

TEST_F(SessionServiceTest, LoadEmptyWindowFromDirectory) {
  SessionServiceIOS* service = [SessionServiceIOS sharedService];
  SessionWindowIOS* sessionWindow =
      [service loadWindowForBrowserState:chrome_browser_state_.get()];
  EXPECT_TRUE(sessionWindow == nil);
}

TEST_F(SessionServiceTest, LoadWindowFromDirectory) {
  SessionServiceIOS* service = [SessionServiceIOS sharedService];
  base::scoped_nsobject<SessionWindowIOS> origSessionWindow(
      [[SessionWindowIOS alloc] init]);
  [service performSaveWindow:origSessionWindow toDirectory:directoryName_];

  SessionWindowIOS* sessionWindow =
      [service loadWindowForBrowserState:chrome_browser_state_.get()];
  EXPECT_TRUE(sessionWindow != nil);
  EXPECT_EQ(NSNotFound, static_cast<NSInteger>(sessionWindow.selectedIndex));
  EXPECT_EQ(0U, sessionWindow.sessions.count);
}

TEST_F(SessionServiceTest, LoadCorruptedWindow) {
  SessionWindowIOS* sessionWindow =
      LoadSessionFromTestDataFile(FILE_PATH_LITERAL("corrupted.plist"));
  EXPECT_TRUE(sessionWindow == nil);
}

}  // anonymous namespace
