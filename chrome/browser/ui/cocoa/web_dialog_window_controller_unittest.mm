// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/web_dialog_window_controller.h"

#include <string>
#include <vector>

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

using content::WebContents;
using content::WebUIMessageHandler;
using ui::WebDialogDelegate;

namespace {

class MockDelegate : public WebDialogDelegate {
public:
  MOCK_CONST_METHOD0(GetDialogModalType, ui::ModalType());
  MOCK_CONST_METHOD0(GetDialogTitle, base::string16());
  MOCK_CONST_METHOD0(GetDialogContentURL, GURL());
  MOCK_CONST_METHOD1(GetWebUIMessageHandlers,
                     void(std::vector<WebUIMessageHandler*>*));
  MOCK_CONST_METHOD1(GetDialogSize, void(gfx::Size*));
  MOCK_CONST_METHOD0(GetDialogArgs, std::string());
  MOCK_METHOD1(OnDialogClosed, void(const std::string& json_retval));
  MOCK_METHOD2(OnCloseContents,
               void(WebContents* source, bool* out_close_dialog));
  MOCK_CONST_METHOD0(ShouldShowDialogTitle, bool());
};

class WebDialogWindowControllerTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
    CocoaTest::BootstrapCocoa();
    title_ = base::ASCIIToUTF16("Mock Title");
    size_ = gfx::Size(50, 100);
    gurl_ = GURL("");
  }

 protected:
  base::string16 title_;
  gfx::Size size_;
  GURL gurl_;

  // Order here is important.
  MockDelegate delegate_;
};

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;

// TODO(akalin): We can't test much more than the below without a real browser.
// In particular, GetWebUIMessageHandlers() and GetDialogArgs() are never
// called. This should be fixed.

TEST_F(WebDialogWindowControllerTest, showDialog) {
  // We want to make sure web_dialog_window_controller below gets
  // destroyed before delegate_, so we specify our own autorelease pool.
  //
  // TODO(dmaclach): Remove this once
  // http://code.google.com/p/chromium/issues/detail?id=26133 is fixed.
  base::mac::ScopedNSAutoreleasePool release_pool;

  EXPECT_CALL(delegate_, GetDialogTitle())
    .WillOnce(Return(title_));
  EXPECT_CALL(delegate_, GetDialogSize(_))
    .WillOnce(SetArgumentPointee<0>(size_));
  EXPECT_CALL(delegate_, GetDialogContentURL())
    .WillOnce(Return(gurl_));
  EXPECT_CALL(delegate_, OnDialogClosed(_))
    .Times(1);

  WebDialogWindowController* web_dialog_window_controller =
    [[WebDialogWindowController alloc] initWithDelegate:&delegate_
                                                context:profile()];

  [web_dialog_window_controller loadDialogContents];
  [web_dialog_window_controller showWindow:nil];
  [web_dialog_window_controller close];
}

}  // namespace
