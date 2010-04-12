// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/cookie_prompt_window_controller.h"
#include "chrome/browser/cookie_modal_dialog.h"
#include "chrome/test/testing_profile.h"

// A mock class which implements just enough functionality to
// act as a radio with a pre-specified selected button.
@interface MockRadioButtonMatrix : NSObject {
 @private
  NSInteger selectedRow_;
}
- (NSInteger)selectedRow;
@end

@implementation MockRadioButtonMatrix

- (id)initWithSelectedRow:(NSInteger)selectedRow {
  if ((self = [super init])) {
    selectedRow_ = selectedRow;
  }
  return self;
}

- (NSInteger)selectedRow {
  return selectedRow_;
}
@end

namespace {

// A subclass of the |CookiePromptModalDialog| that allows tests of
// some of the prompt window controller's functionality without having
// a full environment by overriding select methods and intercepting
// calls that would otherwise rely on behavior only present in a full
// environment.
class CookiePromptModalDialogMock : public CookiePromptModalDialog {
 public:
  CookiePromptModalDialogMock(const GURL& origin,
                              const std::string& cookieLine,
                              HostContentSettingsMap* hostContentSettingsMap);

  virtual void AllowSiteData(bool remember, bool session_expire);
  virtual void BlockSiteData(bool remember);

  bool allow() const { return allow_; }
  bool remember() const { return remember_; }

 private:

  // The result of the block/unblock decision.
  bool allow_;

  // Whether the block/accept decision should be remembered.
  bool remember_;
};

CookiePromptModalDialogMock::CookiePromptModalDialogMock(
    const GURL& origin,
    const std::string& cookieLine,
    HostContentSettingsMap* hostContentSettingsMap)
    : CookiePromptModalDialog(NULL, hostContentSettingsMap, origin, cookieLine,
                              NULL),
      allow_(false),
      remember_(false) {
}

void CookiePromptModalDialogMock::AllowSiteData(bool remember,
                                                bool session_expire) {
  remember_ = remember;
  allow_ = true;
}

void CookiePromptModalDialogMock::BlockSiteData(bool remember) {
  remember_ = remember;
  allow_ = false;
}

class CookiePromptWindowControllerTest : public CocoaTest {
 public:
  CookiePromptWindowControllerTest()
      : ui_thread_(ChromeThread::UI, &message_loop_) {
    hostContentSettingsMap_ = profile_.GetHostContentSettingsMap();
  }

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  TestingProfile profile_;
  scoped_refptr<HostContentSettingsMap> hostContentSettingsMap_;
};

TEST_F(CookiePromptWindowControllerTest, CreateForCookie) {
  GURL url("http://chromium.org");
  std::string cookieLine(
      "PHPSESSID=0123456789abcdef0123456789abcdef; path=/");
  scoped_ptr<CookiePromptModalDialog> dialog(
      new CookiePromptModalDialog(NULL, hostContentSettingsMap_, url,
                                  cookieLine, NULL));
  scoped_nsobject<CookiePromptWindowController> controller(
      [[CookiePromptWindowController alloc] initWithDialog:dialog.get()]);
  EXPECT_TRUE(controller.get());
  EXPECT_TRUE([controller.get() window]);
}

TEST_F(CookiePromptWindowControllerTest, CreateForDatabase) {
  GURL url("http://google.com");
  string16 databaseName(base::SysNSStringToUTF16(@"some database"));
  string16 databaseDescription(base::SysNSStringToUTF16(@"some desc"));
  scoped_ptr<CookiePromptModalDialog> dialog(
      new CookiePromptModalDialog(NULL, hostContentSettingsMap_,
                                  url, databaseName, databaseDescription, 3456,
                                  NULL));
  scoped_nsobject<CookiePromptWindowController> controller(
      [[CookiePromptWindowController alloc] initWithDialog:dialog.get()]);
  EXPECT_TRUE(controller.get());
  EXPECT_TRUE([controller.get() window]);
}

TEST_F(CookiePromptWindowControllerTest, CreateForLocalStorage) {
  GURL url("http://chromium.org");
  string16 key(base::SysNSStringToUTF16(@"key"));
  string16 value(base::SysNSStringToUTF16(@"value"));
  scoped_ptr<CookiePromptModalDialog> dialog(
      new CookiePromptModalDialog(NULL, hostContentSettingsMap_, url, key,
                                  value, NULL));
  scoped_nsobject<CookiePromptWindowController> controller(
      [[CookiePromptWindowController alloc] initWithDialog:dialog.get()]);
  EXPECT_TRUE(controller.get());
  EXPECT_TRUE([controller.get() window]);
}

TEST_F(CookiePromptWindowControllerTest, RememberMyChoiceAllow) {
  GURL url("http://chromium.org");
  std::string cookieLine(
      "PHPSESSID=0123456789abcdef0123456789abcdef; path=/");
  scoped_ptr<CookiePromptModalDialogMock> dialog(
      new CookiePromptModalDialogMock(url, cookieLine,
                                      hostContentSettingsMap_));
  scoped_nsobject<CookiePromptWindowController> controller(
      [[CookiePromptWindowController alloc] initWithDialog:dialog.get()]);
  scoped_nsobject<MockRadioButtonMatrix> checkbox([[MockRadioButtonMatrix alloc]
      initWithSelectedRow:0]);
  [controller.get() setValue:checkbox.get() forKey:@"radioGroupMatrix_"];

  [controller.get() processModalDialogResult:dialog.get()
                                  returnCode:NSAlertFirstButtonReturn];

  // Need to make sure that the retainCount for the mock radio button
  // goes back down to 1--the controller won't do it for us. And
  // even calling setValue:forKey: again with a nil doesn't
  // decrement it. Ugly, but otherwise valgrind complains.
  [checkbox.get() release];

  EXPECT_TRUE(dialog->remember());
  EXPECT_TRUE(dialog->allow());
}

TEST_F(CookiePromptWindowControllerTest, RememberMyChoiceBlock) {
  GURL url("http://codereview.chromium.org");
  std::string cookieLine(
      "PHPSESSID=0123456789abcdef0123456789abcdef; path=/");
  scoped_ptr<CookiePromptModalDialogMock> dialog(
      new CookiePromptModalDialogMock(url, cookieLine,
                                      hostContentSettingsMap_));
  scoped_nsobject<CookiePromptWindowController> controller(
      [[CookiePromptWindowController alloc] initWithDialog:dialog.get()]);
  scoped_nsobject<MockRadioButtonMatrix> checkbox([[MockRadioButtonMatrix alloc]
      initWithSelectedRow:0]);
  [controller.get() setValue:checkbox.get() forKey:@"radioGroupMatrix_"];

  [controller.get() processModalDialogResult:dialog.get()
                                  returnCode:NSAlertSecondButtonReturn];

  // Need to make sure that the retainCount for the mock radio button
  // goes back down to 1--the controller won't do it for us. And
  // even calling setValue:forKey: again with nil doesn't
  // decrement it. Ugly, but otherwise valgrind complains.
  [checkbox.get() release];

  EXPECT_TRUE(dialog->remember());
  EXPECT_FALSE(dialog->allow());
}

TEST_F(CookiePromptWindowControllerTest, DontRememberMyChoiceAllow) {
  GURL url("http://chromium.org");
  std::string cookieLine(
      "PHPSESSID=0123456789abcdef0123456789abcdef; path=/");
  scoped_ptr<CookiePromptModalDialogMock> dialog(
      new CookiePromptModalDialogMock(url, cookieLine,
                                      hostContentSettingsMap_));
  scoped_nsobject<CookiePromptWindowController> controller(
      [[CookiePromptWindowController alloc] initWithDialog:dialog.get()]);
  scoped_nsobject<MockRadioButtonMatrix> checkbox([[MockRadioButtonMatrix alloc]
      initWithSelectedRow:1]);
  [controller.get() setValue:checkbox.get() forKey:@"radioGroupMatrix_"];

  [controller.get() processModalDialogResult:dialog.get()
                                  returnCode:NSAlertFirstButtonReturn];

  // Need to make sure that the retainCount for the mock radio button
  // goes back down to 1--the controller won't do it for us. And
  // even calling setValue:forKey: again with a nil doesn't
  // decrement it. Ugly, but otherwise valgrind complains.
  [checkbox.get() release];

  EXPECT_FALSE(dialog->remember());
  EXPECT_TRUE(dialog->allow());
}

TEST_F(CookiePromptWindowControllerTest, DontRememberMyChoiceBlock) {
  GURL url("http://codereview.chromium.org");
  std::string cookieLine(
      "PHPSESSID=0123456789abcdef0123456789abcdef; path=/");
  scoped_ptr<CookiePromptModalDialogMock> dialog(
      new CookiePromptModalDialogMock(url, cookieLine,
                                      hostContentSettingsMap_));
  scoped_nsobject<CookiePromptWindowController> controller(
      [[CookiePromptWindowController alloc] initWithDialog:dialog.get()]);
  scoped_nsobject<MockRadioButtonMatrix> checkbox([[MockRadioButtonMatrix alloc]
      initWithSelectedRow:1]);
  [controller.get() setValue:checkbox.get() forKey:@"radioGroupMatrix_"];

  [controller.get() processModalDialogResult:dialog.get()
                                  returnCode:NSAlertSecondButtonReturn];

  // Need to make sure that the retainCount for the mock radio button
  // goes back down to 1--the controller won't do it for us. And
  // even calling setValue:forKey: again with a nil doesn't
  // decrement it. Ugly, but otherwise valgrind complains.
  [checkbox.get() release];

  EXPECT_FALSE(dialog->remember());
  EXPECT_FALSE(dialog->allow());
}

}  // namespace
