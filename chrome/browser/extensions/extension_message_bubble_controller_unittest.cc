// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/dev_mode_bubble_controller.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"

namespace extensions {

class TestDelegate {
 public:
  TestDelegate()
      : action_button_callback_count_(0),
        dismiss_button_callback_count_(0),
        link_click_callback_count_(0) {
  }

  // Returns how often the dismiss button has been called.
  size_t action_click_count() {
    return action_button_callback_count_;
  }

  // Returns how often the dismiss button has been called.
  size_t dismiss_click_count() {
    return dismiss_button_callback_count_;
  }

  // Returns how often the link has been clicked.
  size_t link_click_count() {
    return link_click_callback_count_;
  }

 protected:
  size_t action_button_callback_count_;
  size_t dismiss_button_callback_count_;
  size_t link_click_callback_count_;
};

// A test class for the SuspiciousExtensionBubbleController.
class TestSuspiciousExtensionBubbleController
    : public SuspiciousExtensionBubbleController,
      public TestDelegate {
 public:
  explicit TestSuspiciousExtensionBubbleController(Profile* profile)
      : SuspiciousExtensionBubbleController(profile) {
  }

  virtual void OnBubbleAction() OVERRIDE {
    ++action_button_callback_count_;
    SuspiciousExtensionBubbleController::OnBubbleAction();
  }

  virtual void OnBubbleDismiss() OVERRIDE {
    ++dismiss_button_callback_count_;
    SuspiciousExtensionBubbleController::OnBubbleDismiss();
  }

  virtual void OnLinkClicked() OVERRIDE {
    ++link_click_callback_count_;
    SuspiciousExtensionBubbleController::OnLinkClicked();
  }
};

// A test class for the DevModeBubbleController.
class TestDevModeBubbleController
    : public DevModeBubbleController,
      public TestDelegate {
 public:
  explicit TestDevModeBubbleController(Profile* profile)
      : DevModeBubbleController(profile) {
  }

  virtual void OnBubbleAction() OVERRIDE {
    ++action_button_callback_count_;
    DevModeBubbleController::OnBubbleAction();
  }

  virtual void OnBubbleDismiss() OVERRIDE {
    ++dismiss_button_callback_count_;
    DevModeBubbleController::OnBubbleDismiss();
  }

  virtual void OnLinkClicked() OVERRIDE {
    ++link_click_callback_count_;
    DevModeBubbleController::OnLinkClicked();
  }
};

// A fake bubble used for testing the controller. Takes an action that specifies
// what should happen when the bubble is "shown" (the bubble is actually not
// shown, the corresponding action is taken immediately).
class FakeExtensionMessageBubble : public ExtensionMessageBubble {
 public:
  enum ExtensionBubbleAction {
    BUBBLE_ACTION_CLICK_ACTION_BUTTON = 0,
    BUBBLE_ACTION_CLICK_DISMISS_BUTTON,
    BUBBLE_ACTION_CLICK_LINK,
  };

  FakeExtensionMessageBubble() {}

  void set_action_on_show(ExtensionBubbleAction action) {
    action_ = action;
  }

  virtual void Show() OVERRIDE {
    if (action_ == BUBBLE_ACTION_CLICK_ACTION_BUTTON)
      action_callback_.Run();
    else if (action_ == BUBBLE_ACTION_CLICK_DISMISS_BUTTON)
      dismiss_callback_.Run();
    else if (action_ == BUBBLE_ACTION_CLICK_LINK)
      link_callback_.Run();
  }

  virtual void OnActionButtonClicked(const base::Closure& callback) OVERRIDE {
    action_callback_ = callback;
  }

  virtual void OnDismissButtonClicked(const base::Closure& callback) OVERRIDE {
    dismiss_callback_ = callback;
  }

  virtual void OnLinkClicked(const base::Closure& callback) OVERRIDE {
    link_callback_ = callback;
  }

 private:
  ExtensionBubbleAction action_;

  base::Closure action_callback_;
  base::Closure dismiss_callback_;
  base::Closure link_callback_;
};

class ExtensionMessageBubbleTest : public testing::Test {
 public:
  ExtensionMessageBubbleTest() {
    // The two lines of magical incantation required to get the extension
    // service to work inside a unit test and access the extension prefs.
    thread_bundle_.reset(new content::TestBrowserThreadBundle);
    profile_.reset(new TestingProfile);

    static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile()))->CreateExtensionService(
            CommandLine::ForCurrentProcess(),
            base::FilePath(),
            false);
    service_ = profile_->GetExtensionService();
    service_->Init();

    std::string basic_extension =
        "{\"name\": \"Extension #\","
        "\"version\": \"1.0\","
        "\"manifest_version\": 2}";
    std::string basic_extension_with_action =
        "{\"name\": \"Extension #\","
        "\"version\": \"1.0\","
        "\"browser_action\": {"
        "    \"default_title\": \"Default title\""
        "},"
        "\"manifest_version\": 2}";

    std::string extension_data;
    base::ReplaceChars(basic_extension_with_action, "#", "1", &extension_data);
    scoped_refptr<Extension> my_test_extension1(
        CreateExtension(
            Manifest::COMMAND_LINE,
            extension_data,
            "Autogenerated 1"));

    base::ReplaceChars(basic_extension, "#", "2", &extension_data);
    scoped_refptr<Extension> my_test_extension2(
        CreateExtension(
            Manifest::UNPACKED,
            extension_data,
            "Autogenerated 2"));

    base::ReplaceChars(basic_extension, "#", "3", &extension_data);
    scoped_refptr<Extension> regular_extension(
        CreateExtension(
            Manifest::EXTERNAL_POLICY,
            extension_data,
            "Autogenerated 3"));

    extension_id1_ = my_test_extension1->id();
    extension_id2_ = my_test_extension2->id();
    extension_id3_ = regular_extension->id();

    service_->AddExtension(regular_extension);
    service_->AddExtension(my_test_extension1);
    service_->AddExtension(my_test_extension2);
  }
  virtual ~ExtensionMessageBubbleTest() {
    // Make sure the profile is destroyed before the thread bundle.
    profile_.reset(NULL);
  }

  virtual void SetUp() {
    command_line_.reset(new CommandLine(CommandLine::NO_PROGRAM));
  }

 protected:
  Profile* profile() { return profile_.get(); }

  scoped_refptr<Extension> CreateExtension(
      Manifest::Location location,
      const std::string& data,
      const std::string& id) {
    scoped_ptr<base::DictionaryValue> parsed_manifest(
        extension_function_test_utils::ParseDictionary(data));
    return extension_function_test_utils::CreateExtension(
        location,
        parsed_manifest.get(),
        id);
  }

  ExtensionService* service_;
  std::string extension_id1_;
  std::string extension_id2_;
  std::string extension_id3_;

 private:
  scoped_ptr<CommandLine> command_line_;
  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleTest);
};

// The feature this is meant to test is only implemented on Windows.
#if defined(OS_WIN)
#define MAYBE_WipeoutControllerTest WipeoutControllerTest
#else
#define MAYBE_WipeoutControllerTest DISABLED_WipeoutControllerTest
#endif

TEST_F(ExtensionMessageBubbleTest, MAYBE_WipeoutControllerTest) {
  // The test base class adds three extensions, and we control two of them in
  // this test (ids are: extension_id1_ and extension_id2_).
  scoped_ptr<TestSuspiciousExtensionBubbleController> controller(
      new TestSuspiciousExtensionBubbleController(profile()));
  FakeExtensionMessageBubble bubble;
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_DISMISS_BUTTON);

  // Validate that we don't have a suppress value for the extensions.
  ExtensionPrefs* prefs = service_->extension_prefs();
  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(extension_id1_));
  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(extension_id2_));

  EXPECT_FALSE(controller->ShouldShow());
  std::vector<string16> suspicious_extensions = controller->GetExtensionList();
  EXPECT_EQ(0U, suspicious_extensions.size());
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());

  // Now disable an extension, specifying the wipeout flag.
  service_->DisableExtension(extension_id1_,
                             Extension::DISABLE_NOT_VERIFIED);

  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(extension_id1_));
  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(extension_id2_));
  controller.reset(new TestSuspiciousExtensionBubbleController(
      profile()));
  EXPECT_TRUE(controller->ShouldShow());
  suspicious_extensions = controller->GetExtensionList();
  ASSERT_EQ(1U, suspicious_extensions.size());
  EXPECT_TRUE(ASCIIToUTF16("Extension 1") == suspicious_extensions[0]);
  controller->Show(&bubble);  // Simulate showing the bubble.
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(1U, controller->dismiss_click_count());
  // Now the acknowledge flag should be set only for the first extension.
  EXPECT_TRUE(prefs->HasWipeoutBeenAcknowledged(extension_id1_));
  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(extension_id2_));
  // Clear the flag.
  prefs->SetWipeoutAcknowledged(extension_id1_, false);
  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(extension_id1_));

  // Now disable the other extension and exercise the link click code path.
  service_->DisableExtension(extension_id2_,
                             Extension::DISABLE_NOT_VERIFIED);

  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_LINK);
  controller.reset(new TestSuspiciousExtensionBubbleController(
      profile()));
  EXPECT_TRUE(controller->ShouldShow());
  suspicious_extensions = controller->GetExtensionList();
  ASSERT_EQ(2U, suspicious_extensions.size());
  EXPECT_TRUE(ASCIIToUTF16("Extension 1") == suspicious_extensions[1]);
  EXPECT_TRUE(ASCIIToUTF16("Extension 2") == suspicious_extensions[0]);
  controller->Show(&bubble);  // Simulate showing the bubble.
  EXPECT_EQ(1U, controller->link_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_TRUE(prefs->HasWipeoutBeenAcknowledged(extension_id1_));
}

// The feature this is meant to test is only implemented on Windows.
#if defined(OS_WIN)
#define MAYBE_DevModeControllerTest DevModeControllerTest
#else
#define MAYBE_DevModeControllerTest DISABLED_DevModeControllerTest
#endif

TEST_F(ExtensionMessageBubbleTest, MAYBE_DevModeControllerTest) {
  FeatureSwitch::ScopedOverride force_dev_mode_highlighting(
      FeatureSwitch::force_dev_mode_highlighting(), true);
   // The test base class adds three extensions, and we control two of them in
  // this test (ids are: extension_id1_ and extension_id2_). Extension 1 is a
  // regular extension, Extension 2 is UNPACKED so it counts as a DevMode
  // extension.
  scoped_ptr<TestDevModeBubbleController> controller(
      new TestDevModeBubbleController(profile()));

  // The list will contain one enabled unpacked extension.
  EXPECT_TRUE(controller->ShouldShow());
  std::vector<string16> dev_mode_extensions = controller->GetExtensionList();
  ASSERT_EQ(2U, dev_mode_extensions.size());
  EXPECT_TRUE(ASCIIToUTF16("Extension 2") == dev_mode_extensions[0]);
  EXPECT_TRUE(ASCIIToUTF16("Extension 1") == dev_mode_extensions[1]);
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_EQ(0U, controller->action_click_count());

  // Simulate showing the bubble.
  FakeExtensionMessageBubble bubble;
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_DISMISS_BUTTON);
  controller->Show(&bubble);
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->action_click_count());
  EXPECT_EQ(1U, controller->dismiss_click_count());
  EXPECT_TRUE(service_->GetExtensionById(extension_id1_, false) != NULL);
  EXPECT_TRUE(service_->GetExtensionById(extension_id2_, false) != NULL);

  // Do it again, but now press different button (Disable).
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_ACTION_BUTTON);
  controller.reset(new TestDevModeBubbleController(
      profile()));
  EXPECT_TRUE(controller->ShouldShow());
  dev_mode_extensions = controller->GetExtensionList();
  EXPECT_EQ(2U, dev_mode_extensions.size());
  controller->Show(&bubble);  // Simulate showing the bubble.
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(1U, controller->action_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_TRUE(service_->GetExtensionById(extension_id1_, false) == NULL);
  EXPECT_TRUE(service_->GetExtensionById(extension_id2_, false) == NULL);

  // Re-enable the extensions (disabled by the action button above).
  service_->EnableExtension(extension_id1_);
  service_->EnableExtension(extension_id2_);

  // Show the dialog a third time, but now press the learn more link.
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_LINK);
  controller.reset(new TestDevModeBubbleController(
      profile()));
  EXPECT_TRUE(controller->ShouldShow());
  dev_mode_extensions = controller->GetExtensionList();
  EXPECT_EQ(2U, dev_mode_extensions.size());
  controller->Show(&bubble);  // Simulate showing the bubble.
  EXPECT_EQ(1U, controller->link_click_count());
  EXPECT_EQ(0U, controller->action_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_TRUE(service_->GetExtensionById(extension_id1_, false) != NULL);
  EXPECT_TRUE(service_->GetExtensionById(extension_id2_, false) != NULL);

  // Now disable the unpacked extension.
  service_->DisableExtension(extension_id1_, Extension::DISABLE_USER_ACTION);
  service_->DisableExtension(extension_id2_, Extension::DISABLE_USER_ACTION);

  controller.reset(new TestDevModeBubbleController(
      profile()));
  EXPECT_FALSE(controller->ShouldShow());
  dev_mode_extensions = controller->GetExtensionList();
  EXPECT_EQ(0U, dev_mode_extensions.size());
}

}  // namespace extensions
