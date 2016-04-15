// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_controller.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/desktop_media_list_observer.h"
#include "chrome/browser/media/fake_desktop_media_list.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest_mac.h"

@interface DesktopMediaPickerController (ExposedForTesting)
- (IKImageBrowserView*)sourceBrowser;
- (NSButton*)shareButton;
- (NSButton*)audioShareCheckbox;
- (NSArray*)items;
@end

@implementation DesktopMediaPickerController (ExposedForTesting)
- (IKImageBrowserView*)sourceBrowser {
  return sourceBrowser_;
}

- (NSButton*)shareButton {
  return shareButton_;
}

- (NSButton*)cancelButton {
  return cancelButton_;
}

- (NSButton*)audioShareCheckbox {
  return audioShareCheckbox_;
}

- (NSArray*)items {
  return items_;
}
@end

class DesktopMediaPickerControllerTest : public CocoaTest {
 public:
  DesktopMediaPickerControllerTest() {}

  void SetUp() override {
    CocoaTest::SetUp();

    screen_list_ = new FakeDesktopMediaList();
    window_list_ = new FakeDesktopMediaList();
    tab_list_ = new FakeDesktopMediaList();

    DesktopMediaPicker::DoneCallback callback =
        base::Bind(&DesktopMediaPickerControllerTest::OnResult,
                   base::Unretained(this));

    controller_.reset([[DesktopMediaPickerController alloc]
        initWithScreenList:std::unique_ptr<DesktopMediaList>(screen_list_)
                windowList:std::unique_ptr<DesktopMediaList>(window_list_)
                   tabList:std::unique_ptr<DesktopMediaList>(tab_list_)
                    parent:nil
                  callback:callback
                   appName:base::ASCIIToUTF16("Screenshare Test")
                targetName:base::ASCIIToUTF16("https://foo.com")
              requestAudio:true]);
  }

  void TearDown() override {
    controller_.reset();
    CocoaTest::TearDown();
  }

  bool WaitForCallback() {
    if (!callback_called_) {
      base::RunLoop().RunUntilIdle();
    }
    return callback_called_;
  }

 protected:
  void OnResult(content::DesktopMediaID source) {
    EXPECT_FALSE(callback_called_);
    callback_called_ = true;
    source_reported_ = source;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  bool callback_called_ = false;
  content::DesktopMediaID source_reported_;
  FakeDesktopMediaList* screen_list_ = nullptr;
  FakeDesktopMediaList* window_list_ = nullptr;
  FakeDesktopMediaList* tab_list_ = nullptr;
  base::scoped_nsobject<DesktopMediaPickerController> controller_;
};

TEST_F(DesktopMediaPickerControllerTest, ShowAndDismiss) {
  [controller_ showWindow:nil];

  window_list_->AddSource(0);
  window_list_->AddSource(1);
  window_list_->SetSourceThumbnail(1);

  NSArray* items = [controller_ items];
  EXPECT_EQ(2U, [items count]);
  EXPECT_NSEQ(@"0", [[items objectAtIndex:0] imageTitle]);
  EXPECT_EQ(nil, [[items objectAtIndex:0] imageRepresentation]);
  EXPECT_NSEQ(@"1", [[items objectAtIndex:1] imageTitle]);
  EXPECT_TRUE([[items objectAtIndex:1] imageRepresentation] != nil);
}

TEST_F(DesktopMediaPickerControllerTest, ClickShare) {
  [controller_ showWindow:nil];

  window_list_->AddSource(0);
  window_list_->SetSourceThumbnail(0);
  window_list_->AddSource(1);
  window_list_->SetSourceThumbnail(1);

  EXPECT_EQ(2U, [[controller_ items] count]);
  EXPECT_FALSE([[controller_ shareButton] isEnabled]);

  NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:1];
  [[controller_ sourceBrowser] setSelectionIndexes:indexSet
                              byExtendingSelection:NO];
  EXPECT_TRUE([[controller_ shareButton] isEnabled]);

  [[controller_ shareButton] performClick:nil];
  EXPECT_TRUE(WaitForCallback());
  EXPECT_EQ(window_list_->GetSource(1).id, source_reported_);
}

TEST_F(DesktopMediaPickerControllerTest, ClickCancel) {
  [controller_ showWindow:nil];

  window_list_->AddSource(0);
  window_list_->SetSourceThumbnail(0);
  window_list_->AddSource(1);
  window_list_->SetSourceThumbnail(1);

  [[controller_ cancelButton] performClick:nil];
  EXPECT_TRUE(WaitForCallback());
  EXPECT_EQ(content::DesktopMediaID(), source_reported_);
}

TEST_F(DesktopMediaPickerControllerTest, CloseWindow) {
  [controller_ showWindow:nil];

  window_list_->AddSource(0);
  window_list_->SetSourceThumbnail(0);
  window_list_->AddSource(1);
  window_list_->SetSourceThumbnail(1);

  [controller_ close];
  EXPECT_TRUE(WaitForCallback());
  EXPECT_EQ(content::DesktopMediaID(), source_reported_);
}

TEST_F(DesktopMediaPickerControllerTest, UpdateThumbnail) {
  [controller_ showWindow:nil];

  window_list_->AddSource(0);
  window_list_->SetSourceThumbnail(0);
  window_list_->AddSource(1);
  window_list_->SetSourceThumbnail(1);

  NSArray* items = [controller_ items];
  EXPECT_EQ(2U, [items count]);
  NSUInteger version = [[items objectAtIndex:0] imageVersion];

  window_list_->SetSourceThumbnail(0);
  EXPECT_NE(version, [[items objectAtIndex:0] imageVersion]);
}

TEST_F(DesktopMediaPickerControllerTest, UpdateName) {
  [controller_ showWindow:nil];

  window_list_->AddSource(0);
  window_list_->SetSourceThumbnail(0);
  window_list_->AddSource(1);
  window_list_->SetSourceThumbnail(1);

  NSArray* items = [controller_ items];
  EXPECT_EQ(2U, [items count]);
  NSUInteger version = [[items objectAtIndex:0] imageVersion];

  window_list_->SetSourceThumbnail(0);
  EXPECT_NE(version, [[items objectAtIndex:0] imageVersion]);
}

TEST_F(DesktopMediaPickerControllerTest, RemoveSource) {
  [controller_ showWindow:nil];

  window_list_->AddSource(0);
  window_list_->AddSource(1);
  window_list_->AddSource(2);
  window_list_->SetSourceName(1, base::ASCIIToUTF16("foo"));

  NSArray* items = [controller_ items];
  EXPECT_EQ(3U, [items count]);
  EXPECT_NSEQ(@"foo", [[items objectAtIndex:1] imageTitle]);
}

TEST_F(DesktopMediaPickerControllerTest, MoveSource) {
  [controller_ showWindow:nil];

  window_list_->AddSource(0);
  window_list_->AddSource(1);
  window_list_->SetSourceName(1, base::ASCIIToUTF16("foo"));
  NSArray* items = [controller_ items];
  EXPECT_NSEQ(@"foo", [[items objectAtIndex:1] imageTitle]);

  window_list_->MoveSource(1, 0);
  EXPECT_NSEQ(@"foo", [[items objectAtIndex:0] imageTitle]);

  window_list_->MoveSource(0, 1);
  EXPECT_NSEQ(@"foo", [[items objectAtIndex:1] imageTitle]);
}

// Make sure the audio share checkbox' state reacts correctly with
// the source selection. Namely the checkbox is enabled only for tab
// sharing on Mac.
TEST_F(DesktopMediaPickerControllerTest, AudioShareCheckboxState) {
  [controller_ showWindow:nil];

  screen_list_->AddSourceByFullMediaID(
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, 0));
  window_list_->AddSourceByFullMediaID(
      content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW, 1));
  tab_list_->AddSourceByFullMediaID(
      content::DesktopMediaID(content::DesktopMediaID::TYPE_WEB_CONTENTS, 2));

  NSButton* checkbox = [controller_ audioShareCheckbox];
  EXPECT_EQ(NO, [checkbox isEnabled]);

  NSIndexSet* index_set = [NSIndexSet indexSetWithIndex:0];
  [checkbox setEnabled:YES];
  [[controller_ sourceBrowser] setSelectionIndexes:index_set
                              byExtendingSelection:NO];
  EXPECT_EQ(NO, [checkbox isEnabled]);

  index_set = [NSIndexSet indexSetWithIndex:1];
  [checkbox setEnabled:YES];
  [[controller_ sourceBrowser] setSelectionIndexes:index_set
                              byExtendingSelection:NO];
  EXPECT_EQ(NO, [checkbox isEnabled]);

  index_set = [NSIndexSet indexSetWithIndex:2];
  [[controller_ sourceBrowser] setSelectionIndexes:index_set
                              byExtendingSelection:NO];
  EXPECT_EQ(YES, [checkbox isEnabled]);

  index_set = [NSIndexSet indexSet];
  [[controller_ sourceBrowser] setSelectionIndexes:index_set
                              byExtendingSelection:NO];
  EXPECT_EQ(NO, [checkbox isEnabled]);
}
