// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_controller.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/run_loop.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest_mac.h"

@interface DesktopMediaPickerController (ExposedForTesting)
- (IKImageBrowserView*)sourceBrowser;
- (NSButton*)okButton;
- (NSArray*)items;
@end

@implementation DesktopMediaPickerController (ExposedForTesting)
- (IKImageBrowserView*)sourceBrowser {
  return sourceBrowser_;
}

- (NSButton*)okButton {
  return okButton_;
}

- (NSButton*)cancelButton {
  return cancelButton_;
}

- (NSArray*)items {
  return items_;
}
@end

class FakeDesktopMediaPickerModel : public DesktopMediaPickerModel {
 public:
  FakeDesktopMediaPickerModel() : observer_(NULL) {
  }

  void AddSource(int id) {
    Source source(
        content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW, id),
        base::Int64ToString16(id));

    sources_.push_back(source);
    observer_->OnSourceAdded(sources_.size() - 1);
  }

  void RemoveSource(int index) {
    sources_.erase(sources_.begin() + index);
    observer_->OnSourceRemoved(sources_.size() - 1);
  }

  void SetSourceThumbnail(int index) {
    sources_[index].thumbnail = thumbnail_;
    observer_->OnSourceThumbnailChanged(index);
  }

  void SetSourceName(int index, string16 name) {
    sources_[index].name = name;
    observer_->OnSourceNameChanged(index);
  }

  // DesktopMediaPickerModel implementation:
  virtual void SetUpdatePeriod(base::TimeDelta period) OVERRIDE {
  }

  virtual void SetThumbnailSize(const gfx::Size& thumbnail_size) OVERRIDE {
  }

  virtual void SetViewDialogWindowId(
      content::DesktopMediaID::Id dialog_id) OVERRIDE {
  }

  virtual void StartUpdating(Observer* observer) OVERRIDE {
    observer_ = observer;

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 150, 150);
    bitmap.allocPixels();
    bitmap.eraseRGB(0, 255, 0);
    thumbnail_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

  virtual int source_count() const OVERRIDE {
    return sources_.size();
  }

  virtual const Source& source(int index) const OVERRIDE {
    return sources_[index];
  }

 private:
  std::vector<Source> sources_;
  Observer* observer_;
  gfx::ImageSkia thumbnail_;
};

class DesktopMediaPickerControllerTest : public CocoaTest {
 public:
  DesktopMediaPickerControllerTest() : callback_called_(false), model_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    CocoaTest::SetUp();

    model_ = new FakeDesktopMediaPickerModel();

    DesktopMediaPicker::DoneCallback callback =
        base::Bind(&DesktopMediaPickerControllerTest::OnResult,
                   base::Unretained(this));

    controller_.reset(
        [[DesktopMediaPickerController alloc]
            initWithModel:scoped_ptr<DesktopMediaPickerModel>(model_)
                 callback:callback
                  appName:ASCIIToUTF16("Screenshare Test")]);
  }

  virtual void TearDown() OVERRIDE {
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
  bool callback_called_;
  content::DesktopMediaID source_reported_;
  FakeDesktopMediaPickerModel* model_;
  base::scoped_nsobject<DesktopMediaPickerController> controller_;
};

TEST_F(DesktopMediaPickerControllerTest, ShowAndDismiss) {
  [controller_ showWindow:nil];

  model_->AddSource(0);
  model_->AddSource(1);
  model_->SetSourceThumbnail(1);

  NSArray* items = [controller_ items];
  EXPECT_EQ(2U, [items count]);
  EXPECT_NSEQ(@"0", [[items objectAtIndex:0] imageTitle]);
  EXPECT_EQ(nil, [[items objectAtIndex:0] imageRepresentation]);
  EXPECT_NSEQ(@"1", [[items objectAtIndex:1] imageTitle]);
  EXPECT_TRUE([[items objectAtIndex:1] imageRepresentation] != nil);
}

TEST_F(DesktopMediaPickerControllerTest, ClickOK) {
  [controller_ showWindow:nil];

  model_->AddSource(0);
  model_->SetSourceThumbnail(0);
  model_->AddSource(1);
  model_->SetSourceThumbnail(1);

  EXPECT_EQ(2U, [[controller_ items] count]);
  EXPECT_FALSE([[controller_ okButton] isEnabled]);

  NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:1];
  [[controller_ sourceBrowser] setSelectionIndexes:indexSet
                              byExtendingSelection:NO];
  EXPECT_TRUE([[controller_ okButton] isEnabled]);

  [[controller_ okButton] performClick:nil];
  EXPECT_TRUE(WaitForCallback());
  EXPECT_EQ(model_->source(1).id, source_reported_);
}

TEST_F(DesktopMediaPickerControllerTest, ClickCancel) {
  [controller_ showWindow:nil];

  model_->AddSource(0);
  model_->SetSourceThumbnail(0);
  model_->AddSource(1);
  model_->SetSourceThumbnail(1);

  [[controller_ cancelButton] performClick:nil];
  EXPECT_TRUE(WaitForCallback());
  EXPECT_EQ(content::DesktopMediaID(), source_reported_);
}

TEST_F(DesktopMediaPickerControllerTest, CloseWindow) {
  [controller_ showWindow:nil];

  model_->AddSource(0);
  model_->SetSourceThumbnail(0);
  model_->AddSource(1);
  model_->SetSourceThumbnail(1);

  [controller_ close];
  EXPECT_TRUE(WaitForCallback());
  EXPECT_EQ(content::DesktopMediaID(), source_reported_);
}

TEST_F(DesktopMediaPickerControllerTest, UpdateThumbnail) {
  [controller_ showWindow:nil];

  model_->AddSource(0);
  model_->SetSourceThumbnail(0);
  model_->AddSource(1);
  model_->SetSourceThumbnail(1);

  NSArray* items = [controller_ items];
  EXPECT_EQ(2U, [items count]);
  NSUInteger version = [[items objectAtIndex:0] imageVersion];

  model_->SetSourceThumbnail(0);
  EXPECT_NE(version, [[items objectAtIndex:0] imageVersion]);
}

TEST_F(DesktopMediaPickerControllerTest, UpdateName) {
  [controller_ showWindow:nil];

  model_->AddSource(0);
  model_->SetSourceThumbnail(0);
  model_->AddSource(1);
  model_->SetSourceThumbnail(1);

  NSArray* items = [controller_ items];
  EXPECT_EQ(2U, [items count]);
  NSUInteger version = [[items objectAtIndex:0] imageVersion];

  model_->SetSourceThumbnail(0);
  EXPECT_NE(version, [[items objectAtIndex:0] imageVersion]);
}

TEST_F(DesktopMediaPickerControllerTest, RemoveSource) {
  [controller_ showWindow:nil];

  model_->AddSource(0);
  model_->AddSource(1);
  model_->AddSource(2);
  model_->SetSourceName(1, ASCIIToUTF16("foo"));

  NSArray* items = [controller_ items];
  EXPECT_EQ(3U, [items count]);
  EXPECT_NSEQ(@"foo", [[items objectAtIndex:1] imageTitle]);
}
