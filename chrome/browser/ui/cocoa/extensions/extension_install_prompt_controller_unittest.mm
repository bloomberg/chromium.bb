// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#import "chrome/browser/extensions/extension_install_ui.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_prompt_controller.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/image_decoder.h"


// Base class for our tests.
class ExtensionInstallPromptControllerTest : public CocoaTest {
public:
  ExtensionInstallPromptControllerTest() {
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("extensions")
                                   .AppendASCII("install_prompt");

    LoadIcon();
    LoadExtension();
  }

 protected:
  void LoadIcon() {
    std::string file_contents;
    file_util::ReadFileToString(test_data_dir_.AppendASCII("icon.png"),
                                &file_contents);

    webkit_glue::ImageDecoder decoder;
    icon_ = decoder.Decode(
        reinterpret_cast<const unsigned char*>(file_contents.c_str()),
        file_contents.length());
  }

  void LoadExtension() {
    FilePath path = test_data_dir_.AppendASCII("extension.json");

    std::string error;
    JSONFileValueSerializer serializer(path);
    scoped_ptr<DictionaryValue> value(static_cast<DictionaryValue*>(
        serializer.Deserialize(NULL, &error)));
    if (!value.get()) {
      LOG(ERROR) << error;
      return;
    }

    extension_ = Extension::Create(
        path.DirName(), Extension::INVALID, *value, false, true, &error);
    if (!extension_.get()) {
      LOG(ERROR) << error;
      return;
    }
  }

  BrowserTestHelper helper_;
  FilePath test_data_dir_;
  SkBitmap icon_;
  scoped_refptr<Extension> extension_;
};


// Mock out the ExtensionInstallUI::Delegate interface so we can ensure the
// dialog is interacting with it correctly.
class MockExtensionInstallUIDelegate : public ExtensionInstallUI::Delegate {
 public:
  MockExtensionInstallUIDelegate()
      : proceed_count_(0),
        abort_count_(0) {}

  // ExtensionInstallUI::Delegate overrides.
  virtual void InstallUIProceed() {
    proceed_count_++;
  }

  virtual void InstallUIAbort() {
    abort_count_++;
  }

  int proceed_count() { return proceed_count_; }
  int abort_count() { return abort_count_; }

 protected:
  int proceed_count_;
  int abort_count_;
};

// Test that we can load the two kinds of prompts correctly, that the outlets
// are hooked up, and that the dialog calls cancel when cancel is pressed.
TEST_F(ExtensionInstallPromptControllerTest, BasicsNormalCancel) {
  scoped_ptr<MockExtensionInstallUIDelegate> delegate(
      new MockExtensionInstallUIDelegate);

  std::vector<string16> warnings;
  warnings.push_back(UTF8ToUTF16("warning 1"));

  scoped_nsobject<ExtensionInstallPromptController>
    controller([[ExtensionInstallPromptController alloc]
                 initWithParentWindow:test_window()
                              profile:helper_.profile()
                            extension:extension_.get()
                            delegate:delegate.get()
                                icon:&icon_
                            warnings:warnings
                                type:ExtensionInstallUI::INSTALL_PROMPT]);

  [controller window];  // force nib load

  // Test the right nib loaded.
  EXPECT_NSEQ(@"ExtensionInstallPrompt", [controller windowNibName]);

  // Check all the controls.
  // Make sure everything is non-nil, and that the fields that are
  // auto-translated don't start with a caret (that would indicate that they
  // were not translated).
  EXPECT_TRUE([controller iconView] != nil);
  EXPECT_TRUE([[controller iconView] image] != nil);

  EXPECT_TRUE([controller titleField] != nil);
  EXPECT_NE(0u, [[[controller titleField] stringValue] length]);

  EXPECT_TRUE([controller subtitleField] != nil);
  EXPECT_NE(0u, [[[controller subtitleField] stringValue] length]);
  EXPECT_NE('^', [[[controller subtitleField] stringValue] characterAtIndex:0]);

  EXPECT_TRUE([controller warningsField] != nil);
  EXPECT_NSEQ([[controller warningsField] stringValue],
              base::SysUTF16ToNSString(warnings[0]));

  EXPECT_TRUE([controller warningsBox] != nil);

  EXPECT_TRUE([controller cancelButton] != nil);
  EXPECT_NE(0u, [[[controller cancelButton] stringValue] length]);
  EXPECT_NE('^', [[[controller cancelButton] stringValue] characterAtIndex:0]);

  EXPECT_TRUE([controller okButton] != nil);
  EXPECT_NE(0u, [[[controller okButton] stringValue] length]);
  EXPECT_NE('^', [[[controller okButton] stringValue] characterAtIndex:0]);

  // Test that cancel calls our delegate.
  [controller cancel:nil];
  EXPECT_EQ(1, delegate->abort_count());
  EXPECT_EQ(0, delegate->proceed_count());
}


TEST_F(ExtensionInstallPromptControllerTest, BasicsNormalOK) {
  scoped_ptr<MockExtensionInstallUIDelegate> delegate(
      new MockExtensionInstallUIDelegate);

  std::vector<string16> warnings;
  warnings.push_back(UTF8ToUTF16("warning 1"));

  scoped_nsobject<ExtensionInstallPromptController>
  controller([[ExtensionInstallPromptController alloc]
              initWithParentWindow:test_window()
              profile:helper_.profile()
              extension:extension_.get()
              delegate:delegate.get()
              icon:&icon_
              warnings:warnings
              type:ExtensionInstallUI::INSTALL_PROMPT]);

  [controller window];  // force nib load
  [controller ok:nil];

  EXPECT_EQ(0, delegate->abort_count());
  EXPECT_EQ(1, delegate->proceed_count());
}

// Test that controls get repositioned when there are two warnings vs one
// warning.
TEST_F(ExtensionInstallPromptControllerTest, MultipleWarnings) {
  scoped_ptr<MockExtensionInstallUIDelegate> delegate1(
      new MockExtensionInstallUIDelegate);
  scoped_ptr<MockExtensionInstallUIDelegate> delegate2(
      new MockExtensionInstallUIDelegate);

  std::vector<string16> one_warning;
  one_warning.push_back(UTF8ToUTF16("warning 1"));

  std::vector<string16> two_warnings;
  two_warnings.push_back(UTF8ToUTF16("warning 1"));
  two_warnings.push_back(UTF8ToUTF16("warning 2"));

  scoped_nsobject<ExtensionInstallPromptController>
  controller1([[ExtensionInstallPromptController alloc]
              initWithParentWindow:test_window()
              profile:helper_.profile()
              extension:extension_.get()
              delegate:delegate1.get()
              icon:&icon_
              warnings:one_warning
              type:ExtensionInstallUI::INSTALL_PROMPT]);

  [controller1 window];  // force nib load

  scoped_nsobject<ExtensionInstallPromptController>
  controller2([[ExtensionInstallPromptController alloc]
               initWithParentWindow:test_window()
               profile:helper_.profile()
               extension:extension_.get()
               delegate:delegate2.get()
               icon:&icon_
               warnings:two_warnings
               type:ExtensionInstallUI::INSTALL_PROMPT]);

  [controller2 window];  // force nib load

  // Test control positioning. We don't test exact positioning because we don't
  // want this to depend on string details and localization. But we do know the
  // relative effect that adding a second warning should have on the layout.
  ASSERT_LT([[controller1 window] frame].size.height,
            [[controller2 window] frame].size.height);

  ASSERT_LT([[controller1 warningsField] frame].size.height,
            [[controller2 warningsField] frame].size.height);

  ASSERT_LT([[controller1 warningsBox] frame].size.height,
            [[controller2 warningsBox] frame].size.height);

  ASSERT_EQ([[controller1 warningsBox] frame].origin.y,
            [[controller2 warningsBox] frame].origin.y);

  ASSERT_LT([[controller1 subtitleField] frame].origin.y,
            [[controller2 subtitleField] frame].origin.y);

  ASSERT_LT([[controller1 titleField] frame].origin.y,
            [[controller2 titleField] frame].origin.y);
}

// Test that we can load the skinny prompt correctly, and that the outlets are
// are hooked up.
TEST_F(ExtensionInstallPromptControllerTest, BasicsSkinny) {
  scoped_ptr<MockExtensionInstallUIDelegate> delegate(
      new MockExtensionInstallUIDelegate);

  // No warnings should trigger skinny prompt.
  std::vector<string16> warnings;

  scoped_nsobject<ExtensionInstallPromptController>
  controller([[ExtensionInstallPromptController alloc]
              initWithParentWindow:test_window()
              profile:helper_.profile()
              extension:extension_.get()
              delegate:delegate.get()
              icon:&icon_
              warnings:warnings
              type:ExtensionInstallUI::INSTALL_PROMPT]);

  [controller window];  // force nib load

  // Test the right nib loaded.
  EXPECT_NSEQ(@"ExtensionInstallPromptNoWarnings", [controller windowNibName]);

  // Check all the controls.
  // In the skinny prompt, only the icon, title and buttons are non-nill.
  // Everything else is nil.
  EXPECT_TRUE([controller iconView] != nil);
  EXPECT_TRUE([[controller iconView] image] != nil);

  EXPECT_TRUE([controller titleField] != nil);
  EXPECT_NE(0u, [[[controller titleField] stringValue] length]);

  EXPECT_TRUE([controller cancelButton] != nil);
  EXPECT_NE(0u, [[[controller cancelButton] stringValue] length]);
  EXPECT_NE('^', [[[controller cancelButton] stringValue] characterAtIndex:0]);

  EXPECT_TRUE([controller okButton] != nil);
  EXPECT_NE(0u, [[[controller okButton] stringValue] length]);
  EXPECT_NE('^', [[[controller okButton] stringValue] characterAtIndex:0]);

  EXPECT_TRUE([controller subtitleField] == nil);
  EXPECT_TRUE([controller warningsField] == nil);
  EXPECT_TRUE([controller warningsBox] == nil);
}
