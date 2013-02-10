// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/chrome_to_mobile_service.h"
#import "chrome/browser/ui/cocoa/chrome_to_mobile_bubble_controller.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

// Caution!!! Do not conflict with this class name elsewhere!
class MockChromeToMobileService : public ChromeToMobileService {
 public:
  MockChromeToMobileService() : ChromeToMobileService(NULL) {}

  // A utility function to add mock devices.
  void AddDevices(size_t count);

  // ChromeToMobileService overrides:
  virtual const base::ListValue* GetMobiles() const OVERRIDE;

  MOCK_METHOD2(GenerateSnapshot, void(Browser* browser,
                                      base::WeakPtr<Observer> observer));
  MOCK_METHOD4(SendToMobile, void(const base::DictionaryValue* mobile,
                                  const base::FilePath& snapshot,
                                  Browser* browser,
                                  base::WeakPtr<Observer> observer));
  MOCK_METHOD1(DeleteSnapshot, void(const base::FilePath& snapshot));

  // A set of mock mobile devices, kept in lieu of the list in profile prefs.
  base::ListValue mobiles_;
};

void MockChromeToMobileService::AddDevices(size_t count) {
  for(size_t i = 0; i < count; i++) {
    base::DictionaryValue* device = new base::DictionaryValue();
    device->SetString("type", "Device Type");
    device->SetString("name", "Device Name");
    device->SetString("id", "Device ID");
    mobiles_.Append(device);
  }
}

const base::ListValue* MockChromeToMobileService::GetMobiles() const {
  return &mobiles_;
}

namespace {

class ChromeToMobileBubbleControllerTest : public CocoaTest {
 public:
  ChromeToMobileBubbleControllerTest() : controller_(NULL) {}
  virtual ~ChromeToMobileBubbleControllerTest() {}

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

  void CreateBubble() {
    // The controller cleans up after itself when the window closes.
    controller_ = [[ChromeToMobileBubbleController alloc]
        initWithParentWindow:test_window() service:&service_];
    window_ = [controller_ window];
    [controller_ showWindow:nil];
  }

  // Checks the controller's window for the requisite subviews.
  void CheckWindow(int radio_button_count) {
    // Each buble has a title, radio button matrix, and a lower box subview.
    int text_count = 1;
    // The matrix will be hidden if there is only a single device.
    int matrix_count = (radio_button_count > 0) ? 1 : 0;
    int box_count = 1;

    // The window's only immediate child is an invisible view that has a flipped
    // coordinate origin. It is into this that all views get placed.
    NSArray* windowSubviews = [[window_ contentView] subviews];
    EXPECT_EQ(1U, [windowSubviews count]);
    NSArray* subviews = [[windowSubviews lastObject] subviews];
    for (NSView* view in subviews) {
      // Ignore hidden views.
      if ([view isHidden])
        continue;

      if ([view isKindOfClass:[NSTextField class]]) {
        --text_count;
      } else if ([view isKindOfClass:[NSMatrix class]]) {
        --matrix_count;
        NSMatrix* matrix = static_cast<NSMatrix*>(view);
        radio_button_count -= [matrix numberOfRows];
        EXPECT_EQ(1, [matrix numberOfColumns]);
      } else if ([view isKindOfClass:[NSBox class]]) {
        --box_count;
      } else {
        ADD_FAILURE() << "Unknown subview: " << [[view description] UTF8String];
      }
    }

    EXPECT_EQ(0, text_count);
    EXPECT_EQ(0, matrix_count);
    EXPECT_EQ(0, radio_button_count);
    EXPECT_EQ(0, box_count);
    EXPECT_EQ([window_ delegate], controller_);
  }

 protected:
  MockChromeToMobileService service_;
  ChromeToMobileBubbleController* controller_;  // Weak, owns self.

 private:
  NSWindow* window_;  // Weak, owned by controller.

  DISALLOW_COPY_AND_ASSIGN(ChromeToMobileBubbleControllerTest);
};

TEST_F(ChromeToMobileBubbleControllerTest, OneDevice) {
  EXPECT_CALL(service_, GenerateSnapshot(NULL, testing::_));
  EXPECT_CALL(service_, SendToMobile(testing::_, testing::_,
                                     testing::_, testing::_)).Times(0);
  EXPECT_CALL(service_, DeleteSnapshot(testing::_));

  service_.AddDevices(1);
  CreateBubble();
  CheckWindow(/*radio_buttons=*/0);
}

TEST_F(ChromeToMobileBubbleControllerTest, TwoDevices) {
  EXPECT_CALL(service_, GenerateSnapshot(NULL, testing::_));
  EXPECT_CALL(service_, SendToMobile(testing::_, testing::_,
                                     testing::_, testing::_)).Times(0);
  EXPECT_CALL(service_, DeleteSnapshot(testing::_));

  service_.AddDevices(2);
  CreateBubble();
  CheckWindow(/*radio_buttons=*/2);
}

TEST_F(ChromeToMobileBubbleControllerTest, ThreeDevices) {
  EXPECT_CALL(service_, GenerateSnapshot(NULL, testing::_));
  EXPECT_CALL(service_, SendToMobile(testing::_, testing::_,
                                     testing::_, testing::_)).Times(0);
  EXPECT_CALL(service_, DeleteSnapshot(testing::_));

  service_.AddDevices(3);
  CreateBubble();
  CheckWindow(/*radio_buttons=*/3);
}

TEST_F(ChromeToMobileBubbleControllerTest, SendWithoutSnapshot) {
  base::FilePath path;
  EXPECT_CALL(service_, GenerateSnapshot(NULL, testing::_));
  EXPECT_CALL(service_, SendToMobile(testing::_, path, NULL, testing::_));
  EXPECT_CALL(service_, DeleteSnapshot(testing::_));

  service_.AddDevices(1);
  CreateBubble();
  [controller_ send:nil];
}

TEST_F(ChromeToMobileBubbleControllerTest, SendWithSnapshot) {
  base::FilePath path("path.mht");
  EXPECT_CALL(service_, GenerateSnapshot(NULL, testing::_));
  EXPECT_CALL(service_, SendToMobile(testing::_, path, NULL, testing::_));
  EXPECT_CALL(service_, DeleteSnapshot(testing::_));

  service_.AddDevices(1);
  CreateBubble();
  ChromeToMobileBubbleNotificationBridge* bridge = [controller_ bridge];
  bridge->SnapshotGenerated(path, 1);
  [controller_ setSendCopy:YES];
  [controller_ send:nil];
  // Send failure to prevent the bubble from posting a task to close itself.
  bridge->OnSendComplete(false);
}

}  // namespace
