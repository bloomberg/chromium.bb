// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/html_dialog_window_controller.h"

#include <string>
#include <vector>

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/webui/web_ui.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

namespace {

class MockDelegate : public HtmlDialogUIDelegate {
public:
  MOCK_CONST_METHOD0(IsDialogModal, bool());
  MOCK_CONST_METHOD0(GetDialogTitle, std::wstring());
  MOCK_CONST_METHOD0(GetDialogContentURL, GURL());
  MOCK_CONST_METHOD1(GetWebUIMessageHandlers,
                     void(std::vector<WebUIMessageHandler*>*));
  MOCK_CONST_METHOD1(GetDialogSize, void(gfx::Size*));
  MOCK_CONST_METHOD0(GetDialogArgs, std::string());
  MOCK_METHOD1(OnDialogClosed, void(const std::string& json_retval));
  MOCK_METHOD2(OnCloseContents,
               void(TabContents* source, bool* out_close_dialog));
  MOCK_CONST_METHOD0(ShouldShowDialogTitle, bool());
};

class HtmlDialogWindowControllerTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
    CocoaTest::BootstrapCocoa();
    title_ = L"Mock Title";
    size_ = gfx::Size(50, 100);
    gurl_ = GURL("");
  }

 protected:
  std::wstring title_;
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

TEST_F(HtmlDialogWindowControllerTest, showDialog) {
  // We want to make sure html_dialog_window_controller below gets
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

  HtmlDialogWindowController* html_dialog_window_controller =
    [[HtmlDialogWindowController alloc] initWithDelegate:&delegate_
                                                 profile:profile()];

  [html_dialog_window_controller loadDialogContents];
  [html_dialog_window_controller showWindow:nil];
  [html_dialog_window_controller close];
}

}  // namespace
