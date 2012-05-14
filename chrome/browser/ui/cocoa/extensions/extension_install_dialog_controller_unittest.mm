// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#import "base/memory/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#import "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_dialog_controller.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "webkit/glue/image_decoder.h"


// Base class for our tests.
class ExtensionInstallDialogControllerTest : public CocoaProfileTest {
public:
  ExtensionInstallDialogControllerTest() {
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
    SkBitmap bitmap = decoder.Decode(
        reinterpret_cast<const unsigned char*>(file_contents.c_str()),
        file_contents.length());
    icon_ = gfx::Image(new SkBitmap(bitmap));
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

    extension_ = Extension::Create(path.DirName(), Extension::INVALID, *value,
                                   Extension::STRICT_ERROR_CHECKS, &error);
    if (!extension_.get()) {
      LOG(ERROR) << error;
      return;
    }
  }

  FilePath test_data_dir_;
  gfx::Image icon_;
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
  virtual void InstallUIProceed() OVERRIDE {
    proceed_count_++;
  }

  virtual void InstallUIAbort(bool user_initiated) OVERRIDE {
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
TEST_F(ExtensionInstallDialogControllerTest, BasicsNormalCancel) {
  MockExtensionInstallUIDelegate delegate;

  ExtensionInstallUI::Prompt prompt(ExtensionInstallUI::INSTALL_PROMPT);
  std::vector<string16> permissions;
  permissions.push_back(UTF8ToUTF16("warning 1"));
  prompt.SetPermissions(permissions);
  prompt.set_extension(extension_.get());
  prompt.set_icon(icon_);

  scoped_nsobject<ExtensionInstallDialogController>
    controller([[ExtensionInstallDialogController alloc]
                 initWithParentWindow:test_window()
                              profile:profile()
                            delegate:&delegate
                              prompt:prompt]);

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
              base::SysUTF16ToNSString(prompt.GetPermission(0)));

  EXPECT_TRUE([controller cancelButton] != nil);
  EXPECT_NE(0u, [[[controller cancelButton] stringValue] length]);
  EXPECT_NE('^', [[[controller cancelButton] stringValue] characterAtIndex:0]);

  EXPECT_TRUE([controller okButton] != nil);
  EXPECT_NE(0u, [[[controller okButton] stringValue] length]);
  EXPECT_NE('^', [[[controller okButton] stringValue] characterAtIndex:0]);

  // Test that cancel calls our delegate.
  [controller cancel:nil];
  EXPECT_EQ(1, delegate.abort_count());
  EXPECT_EQ(0, delegate.proceed_count());
}


TEST_F(ExtensionInstallDialogControllerTest, BasicsNormalOK) {
  MockExtensionInstallUIDelegate delegate;

  ExtensionInstallUI::Prompt prompt(ExtensionInstallUI::INSTALL_PROMPT);
  std::vector<string16> permissions;
  permissions.push_back(UTF8ToUTF16("warning 1"));
  prompt.SetPermissions(permissions);
  prompt.set_extension(extension_.get());
  prompt.set_icon(icon_);

  scoped_nsobject<ExtensionInstallDialogController>
  controller([[ExtensionInstallDialogController alloc]
               initWithParentWindow:test_window()
                            profile:profile()
                           delegate:&delegate
                             prompt:prompt]);

  [controller window];  // force nib load
  [controller ok:nil];

  EXPECT_EQ(0, delegate.abort_count());
  EXPECT_EQ(1, delegate.proceed_count());
}

// Test that controls get repositioned when there are two warnings vs one
// warning.
TEST_F(ExtensionInstallDialogControllerTest, MultipleWarnings) {
  MockExtensionInstallUIDelegate delegate1;
  MockExtensionInstallUIDelegate delegate2;

  ExtensionInstallUI::Prompt one_warning_prompt(
      ExtensionInstallUI::INSTALL_PROMPT);
  std::vector<string16> permissions;
  permissions.push_back(UTF8ToUTF16("warning 1"));
  one_warning_prompt.SetPermissions(permissions);
  one_warning_prompt.set_extension(extension_.get());
  one_warning_prompt.set_icon(icon_);

  ExtensionInstallUI::Prompt two_warnings_prompt(
      ExtensionInstallUI::INSTALL_PROMPT);
  permissions.push_back(UTF8ToUTF16("warning 2"));
  two_warnings_prompt.SetPermissions(permissions);
  two_warnings_prompt.set_extension(extension_.get());
  two_warnings_prompt.set_icon(icon_);

  scoped_nsobject<ExtensionInstallDialogController>
  controller1([[ExtensionInstallDialogController alloc]
                initWithParentWindow:test_window()
                             profile:profile()
                            delegate:&delegate1
                              prompt:one_warning_prompt]);

  [controller1 window];  // force nib load

  scoped_nsobject<ExtensionInstallDialogController>
  controller2([[ExtensionInstallDialogController alloc]
                initWithParentWindow:test_window()
                             profile:profile()
                            delegate:&delegate2
                              prompt:two_warnings_prompt]);

  [controller2 window];  // force nib load

  // Test control positioning. We don't test exact positioning because we don't
  // want this to depend on string details and localization. But we do know the
  // relative effect that adding a second warning should have on the layout.
  ASSERT_LT([[controller1 window] frame].size.height,
            [[controller2 window] frame].size.height);

  ASSERT_LT([[controller1 warningsField] frame].size.height,
            [[controller2 warningsField] frame].size.height);

  ASSERT_LT([[controller1 subtitleField] frame].origin.y,
            [[controller2 subtitleField] frame].origin.y);

  ASSERT_LT([[controller1 titleField] frame].origin.y,
            [[controller2 titleField] frame].origin.y);
}

// Test that we can load the skinny prompt correctly, and that the outlets are
// are hooked up.
TEST_F(ExtensionInstallDialogControllerTest, BasicsSkinny) {
  MockExtensionInstallUIDelegate delegate;

  // No warnings should trigger skinny prompt.
  ExtensionInstallUI::Prompt no_warnings_prompt(
      ExtensionInstallUI::INSTALL_PROMPT);
  no_warnings_prompt.set_extension(extension_.get());
  no_warnings_prompt.set_icon(icon_);

  scoped_nsobject<ExtensionInstallDialogController>
  controller([[ExtensionInstallDialogController alloc]
               initWithParentWindow:test_window()
                            profile:profile()
                           delegate:&delegate
                             prompt:no_warnings_prompt]);

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
}


// Test that we can load the inline prompt correctly, and that the outlets are
// are hooked up.
TEST_F(ExtensionInstallDialogControllerTest, BasicsInline) {
  MockExtensionInstallUIDelegate delegate;

  // No warnings should trigger skinny prompt.
  ExtensionInstallUI::Prompt inline_prompt(
      ExtensionInstallUI::INLINE_INSTALL_PROMPT);
  inline_prompt.SetInlineInstallWebstoreData("1,000", 3.5, 200);
  inline_prompt.set_extension(extension_.get());
  inline_prompt.set_icon(icon_);

  scoped_nsobject<ExtensionInstallDialogController>
  controller([[ExtensionInstallDialogController alloc]
               initWithParentWindow:test_window()
                            profile:profile()
                           delegate:&delegate
                             prompt:inline_prompt]);

  [controller window];  // force nib load

  // Test the right nib loaded.
  EXPECT_NSEQ(@"ExtensionInstallPromptInline", [controller windowNibName]);

  // Check all the controls.
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

  EXPECT_TRUE([controller ratingStars] != nil);
  EXPECT_EQ(5u, [[[controller ratingStars] subviews] count]);

  EXPECT_TRUE([controller ratingCountField] != nil);
  EXPECT_NE(0u, [[[controller ratingCountField] stringValue] length]);

  EXPECT_TRUE([controller userCountField] != nil);
  EXPECT_NE(0u, [[[controller userCountField] stringValue] length]);

  // Though we have no permissions warnings, these should still be hooked up,
  // just invisible.
  EXPECT_TRUE([controller subtitleField] != nil);
  EXPECT_TRUE([[controller subtitleField] isHidden]);
  EXPECT_TRUE([controller warningsField] != nil);
  EXPECT_TRUE([[controller warningsField] isHidden]);
  EXPECT_TRUE([controller warningsSeparator] != nil);
  EXPECT_TRUE([[controller warningsSeparator] isHidden]);
}
