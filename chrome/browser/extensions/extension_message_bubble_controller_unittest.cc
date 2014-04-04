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
#include "chrome/browser/extensions/settings_api_bubble_controller.h"
#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"

namespace {

const char kId1[] = "iccfkkhkfiphcjdakkmcjmkfboccmndk";
const char kId2[] = "ajjhifimiemdpmophmkkkcijegphclbl";
const char kId3[] = "ioibbbfddncmmabjmpokikkeiofalaek";

}  // namespace

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

// A test class for the SettingsApiBubbleController.
class TestSettingsApiBubbleController : public SettingsApiBubbleController,
                                        public TestDelegate {
 public:
  TestSettingsApiBubbleController(Profile* profile,
                                  SettingsApiOverrideType type)
      : SettingsApiBubbleController(profile, type) {}

  virtual void OnBubbleAction() OVERRIDE {
    ++action_button_callback_count_;
    SettingsApiBubbleController::OnBubbleAction();
  }

  virtual void OnBubbleDismiss() OVERRIDE {
    ++dismiss_button_callback_count_;
    SettingsApiBubbleController::OnBubbleDismiss();
  }

  virtual void OnLinkClicked() OVERRIDE {
    ++link_click_callback_count_;
    SettingsApiBubbleController::OnLinkClicked();
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
  ExtensionMessageBubbleTest() {}

  void LoadGenericExtension(const std::string& index,
                            const std::string& id,
                            Manifest::Location location) {
    extensions::ExtensionBuilder builder;
    builder.SetManifest(extensions::DictionaryBuilder()
                            .Set("name", std::string("Extension " + index))
                            .Set("version", "1.0")
                            .Set("manifest_version", 2));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());
  }

  void LoadExtensionWithAction(const std::string& index,
                               const std::string& id,
                               Manifest::Location location) {
    extensions::ExtensionBuilder builder;
    builder.SetManifest(extensions::DictionaryBuilder()
                            .Set("name", std::string("Extension " + index))
                            .Set("version", "1.0")
                            .Set("manifest_version", 2)
                            .Set("browser_action",
                                 extensions::DictionaryBuilder().Set(
                                     "default_title", "Default title")));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());
  }

  void LoadExtensionOverridingHome(const std::string& index,
                                   const std::string& id,
                                   Manifest::Location location) {
    extensions::ExtensionBuilder builder;
    builder.SetManifest(extensions::DictionaryBuilder()
                            .Set("name", std::string("Extension " + index))
                            .Set("version", "1.0")
                            .Set("manifest_version", 2)
                            .Set("chrome_settings_overrides",
                                 extensions::DictionaryBuilder().Set(
                                     "homepage", "http://www.google.com")));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());
  }

  void LoadExtensionOverridingStart(const std::string& index,
                                    const std::string& id,
                                    Manifest::Location location) {
    extensions::ExtensionBuilder builder;
    builder.SetManifest(extensions::DictionaryBuilder()
                            .Set("name", std::string("Extension " + index))
                            .Set("version", "1.0")
                            .Set("manifest_version", 2)
                            .Set("chrome_settings_overrides",
                                 extensions::DictionaryBuilder().Set(
                                     "startup_pages",
                                     extensions::ListBuilder().Append(
                                         "http://www.google.com"))));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());
  }

  void Init() {
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
  Init();
  // Add three extensions, and control two of them in this test (extension 1
  // and 2).
  LoadExtensionWithAction("1", kId1, Manifest::COMMAND_LINE);
  LoadGenericExtension("2", kId2, Manifest::UNPACKED);
  LoadGenericExtension("3", kId3, Manifest::EXTERNAL_POLICY);

  scoped_ptr<TestSuspiciousExtensionBubbleController> controller(
      new TestSuspiciousExtensionBubbleController(profile()));
  FakeExtensionMessageBubble bubble;
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_DISMISS_BUTTON);

  // Validate that we don't have a suppress value for the extensions.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(kId1));
  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(kId2));

  EXPECT_FALSE(controller->ShouldShow());
  std::vector<base::string16> suspicious_extensions =
      controller->GetExtensionList();
  EXPECT_EQ(0U, suspicious_extensions.size());
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());

  // Now disable an extension, specifying the wipeout flag.
  service_->DisableExtension(kId1, Extension::DISABLE_NOT_VERIFIED);

  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(kId1));
  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(kId2));
  controller.reset(new TestSuspiciousExtensionBubbleController(
      profile()));
  SuspiciousExtensionBubbleController::ClearProfileListForTesting();
  EXPECT_TRUE(controller->ShouldShow());
  suspicious_extensions = controller->GetExtensionList();
  ASSERT_EQ(1U, suspicious_extensions.size());
  EXPECT_TRUE(base::ASCIIToUTF16("Extension 1") == suspicious_extensions[0]);
  controller->Show(&bubble);  // Simulate showing the bubble.
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(1U, controller->dismiss_click_count());
  // Now the acknowledge flag should be set only for the first extension.
  EXPECT_TRUE(prefs->HasWipeoutBeenAcknowledged(kId1));
  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(kId2));
  // Clear the flag.
  prefs->SetWipeoutAcknowledged(kId1, false);
  EXPECT_FALSE(prefs->HasWipeoutBeenAcknowledged(kId1));

  // Now disable the other extension and exercise the link click code path.
  service_->DisableExtension(kId2, Extension::DISABLE_NOT_VERIFIED);

  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_LINK);
  controller.reset(new TestSuspiciousExtensionBubbleController(
      profile()));
  SuspiciousExtensionBubbleController::ClearProfileListForTesting();
  EXPECT_TRUE(controller->ShouldShow());
  suspicious_extensions = controller->GetExtensionList();
  ASSERT_EQ(2U, suspicious_extensions.size());
  EXPECT_TRUE(base::ASCIIToUTF16("Extension 1") == suspicious_extensions[1]);
  EXPECT_TRUE(base::ASCIIToUTF16("Extension 2") == suspicious_extensions[0]);
  controller->Show(&bubble);  // Simulate showing the bubble.
  EXPECT_EQ(1U, controller->link_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_TRUE(prefs->HasWipeoutBeenAcknowledged(kId1));
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
  Init();
  // Add three extensions, and control two of them in this test (extension 1
  // and 2). Extension 1 is a regular extension, Extension 2 is UNPACKED so it
  // counts as a DevMode extension.
  LoadExtensionWithAction("1", kId1, Manifest::COMMAND_LINE);
  LoadGenericExtension("2", kId2, Manifest::UNPACKED);
  LoadGenericExtension("3", kId3, Manifest::EXTERNAL_POLICY);

  scoped_ptr<TestDevModeBubbleController> controller(
      new TestDevModeBubbleController(profile()));

  // The list will contain one enabled unpacked extension.
  EXPECT_TRUE(controller->ShouldShow());
  std::vector<base::string16> dev_mode_extensions =
      controller->GetExtensionList();
  ASSERT_EQ(2U, dev_mode_extensions.size());
  EXPECT_TRUE(base::ASCIIToUTF16("Extension 2") == dev_mode_extensions[0]);
  EXPECT_TRUE(base::ASCIIToUTF16("Extension 1") == dev_mode_extensions[1]);
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
  EXPECT_TRUE(service_->GetExtensionById(kId1, false) != NULL);
  EXPECT_TRUE(service_->GetExtensionById(kId2, false) != NULL);

  // Do it again, but now press different button (Disable).
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_ACTION_BUTTON);
  controller.reset(new TestDevModeBubbleController(
      profile()));
  DevModeBubbleController::ClearProfileListForTesting();
  EXPECT_TRUE(controller->ShouldShow());
  dev_mode_extensions = controller->GetExtensionList();
  EXPECT_EQ(2U, dev_mode_extensions.size());
  controller->Show(&bubble);  // Simulate showing the bubble.
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(1U, controller->action_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_TRUE(service_->GetExtensionById(kId1, false) == NULL);
  EXPECT_TRUE(service_->GetExtensionById(kId2, false) == NULL);

  // Re-enable the extensions (disabled by the action button above).
  service_->EnableExtension(kId1);
  service_->EnableExtension(kId2);

  // Show the dialog a third time, but now press the learn more link.
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_LINK);
  controller.reset(new TestDevModeBubbleController(
      profile()));
  DevModeBubbleController::ClearProfileListForTesting();
  EXPECT_TRUE(controller->ShouldShow());
  dev_mode_extensions = controller->GetExtensionList();
  EXPECT_EQ(2U, dev_mode_extensions.size());
  controller->Show(&bubble);  // Simulate showing the bubble.
  EXPECT_EQ(1U, controller->link_click_count());
  EXPECT_EQ(0U, controller->action_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_TRUE(service_->GetExtensionById(kId1, false) != NULL);
  EXPECT_TRUE(service_->GetExtensionById(kId2, false) != NULL);

  // Now disable the unpacked extension.
  service_->DisableExtension(kId1, Extension::DISABLE_USER_ACTION);
  service_->DisableExtension(kId2, Extension::DISABLE_USER_ACTION);

  controller.reset(new TestDevModeBubbleController(
      profile()));
  DevModeBubbleController::ClearProfileListForTesting();
  EXPECT_FALSE(controller->ShouldShow());
  dev_mode_extensions = controller->GetExtensionList();
  EXPECT_EQ(0U, dev_mode_extensions.size());
}

// The feature this is meant to test is only implemented on Windows.
#if defined(OS_WIN)
#define MAYBE_SettingsApiControllerTest SettingsApiControllerTest
#else
#define MAYBE_SettingsApiControllerTest DISABLED_SettingsApiControllerTest
#endif

TEST_F(ExtensionMessageBubbleTest, MAYBE_SettingsApiControllerTest) {
  // The API this test is exercising has not been release on all channels.
  // TODO(finnur): Remove the if check once it is released.
  if (chrome::VersionInfo::GetChannel() > chrome::VersionInfo::CHANNEL_DEV)
    return;

  Init();
  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());

  for (int i = 0; i < 3; ++i) {
    switch (static_cast<SettingsApiOverrideType>(i)) {
      case BUBBLE_TYPE_HOME_PAGE:
        // Load two extensions overriding home page and one overriding something
        // unrelated (to check for interference). Extension 2 should still win
        // on the home page setting.
        LoadExtensionOverridingHome("1", kId1, Manifest::UNPACKED);
        LoadExtensionOverridingHome("2", kId2, Manifest::UNPACKED);
        LoadExtensionOverridingStart("3", kId3, Manifest::UNPACKED);
        break;
      case BUBBLE_TYPE_SEARCH_ENGINE:
        // We deliberately skip testing the search engine since it relies on
        // TemplateURLServiceFactory that isn't available while unit testing.
        // This test is only simulating the bubble interaction with the user and
        // that is more or less the same for the search engine as it is for the
        // others.
        continue;
      case BUBBLE_TYPE_STARTUP_PAGES:
        // Load two extensions overriding start page and one overriding
        // something unrelated (to check for interference). Extension 2 should
        // still win on the startup page setting.
        LoadExtensionOverridingStart("1", kId1, Manifest::UNPACKED);
        LoadExtensionOverridingStart("2", kId2, Manifest::UNPACKED);
        LoadExtensionOverridingHome("3", kId3, Manifest::UNPACKED);
        break;
      default:
        NOTREACHED();
        break;
    }

    scoped_ptr<TestSettingsApiBubbleController> controller(
        new TestSettingsApiBubbleController(
            profile(), static_cast<SettingsApiOverrideType>(i)));

    // The list will contain one enabled unpacked extension (ext 2).
    EXPECT_TRUE(controller->ShouldShow(kId2));
    std::vector<base::string16> override_extensions =
        controller->GetExtensionList();
    ASSERT_EQ(1U, override_extensions.size());
    EXPECT_TRUE(base::ASCIIToUTF16("Extension 2") ==
                override_extensions[0].c_str());
    EXPECT_EQ(0U, controller->link_click_count());
    EXPECT_EQ(0U, controller->dismiss_click_count());
    EXPECT_EQ(0U, controller->action_click_count());

    // Simulate showing the bubble and dismissing it.
    FakeExtensionMessageBubble bubble;
    bubble.set_action_on_show(
        FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_DISMISS_BUTTON);
    controller->Show(&bubble);
    EXPECT_EQ(0U, controller->link_click_count());
    EXPECT_EQ(0U, controller->action_click_count());
    EXPECT_EQ(1U, controller->dismiss_click_count());
    // No extension should have become disabled.
    EXPECT_TRUE(service_->GetExtensionById(kId1, false) != NULL);
    EXPECT_TRUE(service_->GetExtensionById(kId2, false) != NULL);
    EXPECT_TRUE(service_->GetExtensionById(kId3, false) != NULL);
    // Only extension 2 should have been acknowledged.
    EXPECT_FALSE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId1));
    EXPECT_TRUE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId2));
    EXPECT_FALSE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId3));
    // Clean up after ourselves.
    prefs->SetSettingsApiBubbleBeenAcknowledged(kId2, false);

    // Simulate clicking the learn more link to dismiss it.
    bubble.set_action_on_show(
        FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_LINK);
    controller.reset(new TestSettingsApiBubbleController(
        profile(), static_cast<SettingsApiOverrideType>(i)));
    controller->Show(&bubble);
    EXPECT_EQ(1U, controller->link_click_count());
    EXPECT_EQ(0U, controller->action_click_count());
    EXPECT_EQ(0U, controller->dismiss_click_count());
    // No extension should have become disabled.
    EXPECT_TRUE(service_->GetExtensionById(kId1, false) != NULL);
    EXPECT_TRUE(service_->GetExtensionById(kId2, false) != NULL);
    EXPECT_TRUE(service_->GetExtensionById(kId3, false) != NULL);
    // Only extension 2 should have been acknowledged.
    EXPECT_FALSE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId1));
    EXPECT_TRUE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId2));
    EXPECT_FALSE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId3));
    // Clean up after ourselves.
    prefs->SetSettingsApiBubbleBeenAcknowledged(kId2, false);

    // Do it again, but now opt to disable the extension.
    bubble.set_action_on_show(
        FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_ACTION_BUTTON);
    controller.reset(new TestSettingsApiBubbleController(
        profile(), static_cast<SettingsApiOverrideType>(i)));
    EXPECT_TRUE(controller->ShouldShow(kId2));
    override_extensions = controller->GetExtensionList();
    EXPECT_EQ(1U, override_extensions.size());
    controller->Show(&bubble);  // Simulate showing the bubble.
    EXPECT_EQ(0U, controller->link_click_count());
    EXPECT_EQ(1U, controller->action_click_count());
    EXPECT_EQ(0U, controller->dismiss_click_count());
    // Only extension 2 should have become disabled.
    EXPECT_TRUE(service_->GetExtensionById(kId1, false) != NULL);
    EXPECT_TRUE(service_->GetExtensionById(kId2, false) == NULL);
    EXPECT_TRUE(service_->GetExtensionById(kId3, false) != NULL);
    // No extension should have been acknowledged (it got disabled).
    EXPECT_FALSE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId1));
    EXPECT_FALSE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId2));
    EXPECT_FALSE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId3));

    // Clean up after ourselves.
    service_->UninstallExtension(kId1, false, NULL);
    service_->UninstallExtension(kId2, false, NULL);
    service_->UninstallExtension(kId3, false, NULL);
  }
}

}  // namespace extensions
