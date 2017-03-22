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
#import "ios/chrome/browser/sessions/session_window_ios.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Fixture Class. Takes care of deleting the directory used to store test data.
class SessionServiceTest : public PlatformTest {
 public:
  SessionServiceTest() = default;
  ~SessionServiceTest() override = default;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPath(test_dir_.GetPath());
    chrome_browser_state_ = test_cbs_builder.Build();
    directory_name_.reset([base::SysUTF8ToNSString(
        chrome_browser_state_->GetStatePath().value()) copy]);
  }

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

  ios::ChromeBrowserState* chrome_browser_state() {
    return chrome_browser_state_.get();
  }

  NSString* directory_name() { return directory_name_.get(); }

 private:
  base::ScopedTempDir test_dir_;
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<ios::ChromeBrowserState> chrome_browser_state_;
  base::scoped_nsobject<NSString> directory_name_;

  DISALLOW_COPY_AND_ASSIGN(SessionServiceTest);
};

TEST_F(SessionServiceTest, Singleton) {
  SessionServiceIOS* service = [SessionServiceIOS sharedService];
  EXPECT_TRUE(service != nil);

  SessionServiceIOS* another_service = [SessionServiceIOS sharedService];
  EXPECT_TRUE(another_service != nil);

  EXPECT_TRUE(service == another_service);
}

TEST_F(SessionServiceTest, SaveWindowToDirectory) {
  id session_window_mock =
      [OCMockObject niceMockForClass:[SessionWindowIOS class]];
  SessionServiceIOS* service = [SessionServiceIOS sharedService];
  [service performSaveWindow:session_window_mock toDirectory:directory_name()];

  NSFileManager* file_manager = [NSFileManager defaultManager];
  EXPECT_TRUE([file_manager removeItemAtPath:directory_name() error:nullptr]);
}

TEST_F(SessionServiceTest, SaveWindowToDirectoryAlreadyExistent) {
  id session_window_mock =
      [OCMockObject niceMockForClass:[SessionWindowIOS class]];
  EXPECT_TRUE([[NSFileManager defaultManager]
            createDirectoryAtPath:directory_name()
      withIntermediateDirectories:YES
                       attributes:nil
                            error:nullptr]);

  SessionServiceIOS* service = [SessionServiceIOS sharedService];
  [service performSaveWindow:session_window_mock toDirectory:directory_name()];

  NSFileManager* file_manager = [NSFileManager defaultManager];
  EXPECT_TRUE([file_manager removeItemAtPath:directory_name() error:nullptr]);
}

TEST_F(SessionServiceTest, LoadEmptyWindowFromDirectory) {
  SessionServiceIOS* service = [SessionServiceIOS sharedService];
  SessionWindowIOS* session_window =
      [service loadWindowForBrowserState:chrome_browser_state()];
  EXPECT_TRUE(session_window == nil);
}

TEST_F(SessionServiceTest, LoadWindowFromDirectory) {
  SessionServiceIOS* service = [SessionServiceIOS sharedService];
  base::scoped_nsobject<SessionWindowIOS> origSessionWindow(
      [[SessionWindowIOS alloc] init]);
  [service performSaveWindow:origSessionWindow toDirectory:directory_name()];

  SessionWindowIOS* session_window =
      [service loadWindowForBrowserState:chrome_browser_state()];
  EXPECT_TRUE(session_window != nil);
  EXPECT_EQ(NSNotFound, static_cast<NSInteger>(session_window.selectedIndex));
  EXPECT_EQ(0U, session_window.sessions.count);
}

TEST_F(SessionServiceTest, LoadCorruptedWindow) {
  SessionWindowIOS* session_window =
      LoadSessionFromTestDataFile(FILE_PATH_LITERAL("corrupted.plist"));
  EXPECT_TRUE(session_window == nil);
}

// TODO(crbug.com/661633): remove this once M67 has shipped (i.e. once more
// than a year has passed since the introduction of the compatibility code).
TEST_F(SessionServiceTest, LoadM57Session) {
  SessionWindowIOS* session_window =
      LoadSessionFromTestDataFile(FILE_PATH_LITERAL("session_m57.plist"));
  EXPECT_TRUE(session_window != nil);
}

// TODO(crbug.com/661633): remove this once M68 has shipped (i.e. once more
// than a year has passed since the introduction of the compatibility code).
TEST_F(SessionServiceTest, LoadM58Session) {
  SessionWindowIOS* session_window =
      LoadSessionFromTestDataFile(FILE_PATH_LITERAL("session_m58.plist"));
  EXPECT_TRUE(session_window != nil);
}

}  // anonymous namespace
