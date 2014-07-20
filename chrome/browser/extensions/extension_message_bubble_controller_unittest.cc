// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/dev_mode_bubble_controller.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/ntp_overridden_bubble_controller.h"
#include "chrome/browser/extensions/proxy_overridden_bubble_controller.h"
#include "chrome/browser/extensions/settings_api_bubble_controller.h"
#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/value_builder.h"

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

// A test class for the NtpOverriddenBubbleController.
class TestNtpOverriddenBubbleController
    : public NtpOverriddenBubbleController,
      public TestDelegate {
 public:
  explicit TestNtpOverriddenBubbleController(Profile* profile)
      : NtpOverriddenBubbleController(profile) {
  }

  virtual void OnBubbleAction() OVERRIDE {
    ++action_button_callback_count_;
    NtpOverriddenBubbleController::OnBubbleAction();
  }

  virtual void OnBubbleDismiss() OVERRIDE {
    ++dismiss_button_callback_count_;
    NtpOverriddenBubbleController::OnBubbleDismiss();
  }

  virtual void OnLinkClicked() OVERRIDE {
    ++link_click_callback_count_;
    NtpOverriddenBubbleController::OnLinkClicked();
  }
};

// A test class for the ProxyOverriddenBubbleController.
class TestProxyOverriddenBubbleController
    : public ProxyOverriddenBubbleController,
      public TestDelegate {
 public:
  explicit TestProxyOverriddenBubbleController(Profile* profile)
      : ProxyOverriddenBubbleController(profile) {
  }

  virtual void OnBubbleAction() OVERRIDE {
    ++action_button_callback_count_;
    ProxyOverriddenBubbleController::OnBubbleAction();
  }

  virtual void OnBubbleDismiss() OVERRIDE {
    ++dismiss_button_callback_count_;
    ProxyOverriddenBubbleController::OnBubbleDismiss();
  }

  virtual void OnLinkClicked() OVERRIDE {
    ++link_click_callback_count_;
    ProxyOverriddenBubbleController::OnLinkClicked();
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

  testing::AssertionResult LoadGenericExtension(const std::string& index,
                                                const std::string& id,
                                                Manifest::Location location) {
    ExtensionBuilder builder;
    builder.SetManifest(DictionaryBuilder()
                            .Set("name", std::string("Extension " + index))
                            .Set("version", "1.0")
                            .Set("manifest_version", 2));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
  }

  testing::AssertionResult LoadExtensionWithAction(
      const std::string& index,
      const std::string& id,
      Manifest::Location location) {
    ExtensionBuilder builder;
    builder.SetManifest(DictionaryBuilder()
                            .Set("name", std::string("Extension " + index))
                            .Set("version", "1.0")
                            .Set("manifest_version", 2)
                            .Set("browser_action",
                                 DictionaryBuilder().Set(
                                     "default_title", "Default title")));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
  }

  testing::AssertionResult LoadExtensionOverridingHome(
      const std::string& index,
      const std::string& id,
      Manifest::Location location) {
    ExtensionBuilder builder;
    builder.SetManifest(DictionaryBuilder()
                            .Set("name", std::string("Extension " + index))
                            .Set("version", "1.0")
                            .Set("manifest_version", 2)
                            .Set("chrome_settings_overrides",
                                 DictionaryBuilder().Set(
                                     "homepage", "http://www.google.com")));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
  }

  testing::AssertionResult LoadExtensionOverridingStart(
      const std::string& index,
      const std::string& id,
      Manifest::Location location) {
    ExtensionBuilder builder;
    builder.SetManifest(DictionaryBuilder()
                            .Set("name", std::string("Extension " + index))
                            .Set("version", "1.0")
                            .Set("manifest_version", 2)
                            .Set("chrome_settings_overrides",
                                 DictionaryBuilder().Set(
                                     "startup_pages",
                                     ListBuilder().Append(
                                         "http://www.google.com"))));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
  }

  testing::AssertionResult LoadExtensionOverridingNtp(
      const std::string& index,
      const std::string& id,
      Manifest::Location location) {
    ExtensionBuilder builder;
    builder.SetManifest(DictionaryBuilder()
                            .Set("name", std::string("Extension " + index))
                            .Set("version", "1.0")
                            .Set("manifest_version", 2)
                            .Set("chrome_url_overrides",
                                 DictionaryBuilder().Set(
                                     "newtab", "Default.html")));

    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
  }

  testing::AssertionResult LoadExtensionOverridingProxy(
      const std::string& index,
      const std::string& id,
      Manifest::Location location) {
    ExtensionBuilder builder;
    builder.SetManifest(DictionaryBuilder()
                            .Set("name", std::string("Extension " + index))
                            .Set("version", "1.0")
                            .Set("manifest_version", 2)
                            .Set("permissions",
                                 ListBuilder().Append("proxy")));

    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    // The proxy check relies on ExtensionPrefValueMap being up to date as to
    // specifying which extension is controlling the proxy, but unfortunately
    // that Map is not updated automatically for unit tests, so we simulate the
    // update here to avoid test failures.
    ExtensionPrefValueMap* extension_prefs_value_map =
        ExtensionPrefValueMapFactory::GetForBrowserContext(profile());
    extension_prefs_value_map->RegisterExtension(
        id,
        base::Time::Now(),
        true,    // is_enabled.
        false);  // is_incognito_enabled.
    extension_prefs_value_map->SetExtensionPref(id,
                                                prefs::kProxy,
                                                kExtensionPrefsScopeRegular,
                                                new base::StringValue(id));

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
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
    service_ = ExtensionSystem::Get(profile())->extension_service();
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
  ASSERT_TRUE(LoadExtensionWithAction("1", kId1, Manifest::COMMAND_LINE));
  ASSERT_TRUE(LoadGenericExtension("2", kId2, Manifest::UNPACKED));
  ASSERT_TRUE(LoadGenericExtension("3", kId3, Manifest::EXTERNAL_POLICY));

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
  ASSERT_TRUE(LoadExtensionWithAction("1", kId1, Manifest::COMMAND_LINE));
  ASSERT_TRUE(LoadGenericExtension("2", kId2, Manifest::UNPACKED));
  ASSERT_TRUE(LoadGenericExtension("3", kId3, Manifest::EXTERNAL_POLICY));

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
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != NULL);

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
  EXPECT_TRUE(registry->disabled_extensions().GetByID(kId1) != NULL);
  EXPECT_TRUE(registry->disabled_extensions().GetByID(kId2) != NULL);

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
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != NULL);

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
  Init();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  for (int i = 0; i < 3; ++i) {
    switch (static_cast<SettingsApiOverrideType>(i)) {
      case BUBBLE_TYPE_HOME_PAGE:
        // Load two extensions overriding home page and one overriding something
        // unrelated (to check for interference). Extension 2 should still win
        // on the home page setting.
        ASSERT_TRUE(LoadExtensionOverridingHome("1", kId1, Manifest::UNPACKED));
        ASSERT_TRUE(LoadExtensionOverridingHome("2", kId2, Manifest::UNPACKED));
        ASSERT_TRUE(
            LoadExtensionOverridingStart("3", kId3, Manifest::UNPACKED));
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
        ASSERT_TRUE(
            LoadExtensionOverridingStart("1", kId1, Manifest::UNPACKED));
        ASSERT_TRUE(
            LoadExtensionOverridingStart("2", kId2, Manifest::UNPACKED));
        ASSERT_TRUE(LoadExtensionOverridingHome("3", kId3, Manifest::UNPACKED));
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
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != NULL);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
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
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != NULL);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
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
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
    EXPECT_TRUE(registry->disabled_extensions().GetByID(kId2) != NULL);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
    // No extension should have been acknowledged (it got disabled).
    EXPECT_FALSE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId1));
    EXPECT_FALSE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId2));
    EXPECT_FALSE(prefs->HasSettingsApiBubbleBeenAcknowledged(kId3));

    // Clean up after ourselves.
    service_->UninstallExtension(
        kId1, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
    service_->UninstallExtension(
        kId2, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
    service_->UninstallExtension(
        kId3, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
  }
}

// The feature this is meant to test is only implemented on Windows.
#if defined(OS_WIN)
#define MAYBE_NtpOverriddenControllerTest NtpOverriddenControllerTest
#else
#define MAYBE_NtpOverriddenControllerTest DISABLED_NtpOverriddenControllerTest
#endif

TEST_F(ExtensionMessageBubbleTest, MAYBE_NtpOverriddenControllerTest) {
  Init();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  // Load two extensions overriding new tab page and one overriding something
  // unrelated (to check for interference). Extension 2 should still win
  // on the new tab page setting.
  ASSERT_TRUE(LoadExtensionOverridingNtp("1", kId1, Manifest::UNPACKED));
  ASSERT_TRUE(LoadExtensionOverridingNtp("2", kId2, Manifest::UNPACKED));
  ASSERT_TRUE(LoadExtensionOverridingStart("3", kId3, Manifest::UNPACKED));

  scoped_ptr<TestNtpOverriddenBubbleController> controller(
      new TestNtpOverriddenBubbleController(profile()));

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
  EXPECT_TRUE(controller->ShouldShow(kId2));
  controller->Show(&bubble);
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->action_click_count());
  EXPECT_EQ(1U, controller->dismiss_click_count());
  // No extension should have become disabled.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
  // Only extension 2 should have been acknowledged.
  EXPECT_FALSE(prefs->HasNtpOverriddenBubbleBeenAcknowledged(kId1));
  EXPECT_TRUE(prefs->HasNtpOverriddenBubbleBeenAcknowledged(kId2));
  EXPECT_FALSE(prefs->HasNtpOverriddenBubbleBeenAcknowledged(kId3));
  // Clean up after ourselves.
  prefs->SetNtpOverriddenBubbleBeenAcknowledged(kId2, false);

  // Simulate clicking the learn more link to dismiss it.
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_LINK);
  controller.reset(new TestNtpOverriddenBubbleController(profile()));
  EXPECT_TRUE(controller->ShouldShow(kId2));
  controller->Show(&bubble);
  EXPECT_EQ(1U, controller->link_click_count());
  EXPECT_EQ(0U, controller->action_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  // No extension should have become disabled.
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
  // Only extension 2 should have been acknowledged.
  EXPECT_FALSE(prefs->HasNtpOverriddenBubbleBeenAcknowledged(kId1));
  EXPECT_TRUE(prefs->HasNtpOverriddenBubbleBeenAcknowledged(kId2));
  EXPECT_FALSE(prefs->HasNtpOverriddenBubbleBeenAcknowledged(kId3));
  // Clean up after ourselves.
  prefs->SetNtpOverriddenBubbleBeenAcknowledged(kId2, false);

  // Do it again, but now opt to disable the extension.
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_ACTION_BUTTON);
  controller.reset(new TestNtpOverriddenBubbleController(profile()));
  EXPECT_TRUE(controller->ShouldShow(kId2));
  override_extensions = controller->GetExtensionList();
  EXPECT_EQ(1U, override_extensions.size());
  controller->Show(&bubble);  // Simulate showing the bubble.
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(1U, controller->action_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  // Only extension 2 should have become disabled.
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
  EXPECT_TRUE(registry->disabled_extensions().GetByID(kId2) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
  // No extension should have been acknowledged (it got disabled).
  EXPECT_FALSE(prefs->HasNtpOverriddenBubbleBeenAcknowledged(kId1));
  EXPECT_FALSE(prefs->HasNtpOverriddenBubbleBeenAcknowledged(kId2));
  EXPECT_FALSE(prefs->HasNtpOverriddenBubbleBeenAcknowledged(kId3));

  // Clean up after ourselves.
  service_->UninstallExtension(
      kId1, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
  service_->UninstallExtension(
      kId2, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
  service_->UninstallExtension(
      kId3, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
}

void SetInstallTime(const std::string& extension_id,
                    const base::Time& time,
                    ExtensionPrefs* prefs) {
  std::string time_str = base::Int64ToString(time.ToInternalValue());
  prefs->UpdateExtensionPref(extension_id,
                             "install_time",
                             new base::StringValue(time_str));
}

// The feature this is meant to test is only implemented on Windows.
#if defined(OS_WIN)
#define MAYBE_ProxyOverriddenControllerTest ProxyOverriddenControllerTest
#else
#define MAYBE_ProxyOverriddenControllerTest DISABLED_ProxyOverriddenControllerTest
#endif

TEST_F(ExtensionMessageBubbleTest, MAYBE_ProxyOverriddenControllerTest) {
  Init();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  // Load two extensions overriding proxy and one overriding something
  // unrelated (to check for interference). Extension 2 should still win
  // on the proxy setting.
  ASSERT_TRUE(LoadExtensionOverridingProxy("1", kId1, Manifest::UNPACKED));
  ASSERT_TRUE(LoadExtensionOverridingProxy("2", kId2, Manifest::UNPACKED));
  ASSERT_TRUE(LoadExtensionOverridingStart("3", kId3, Manifest::UNPACKED));

  // The bubble will not show if the extension was installed in the last 7 days
  // so we artificially set the install time to simulate an old install during
  // testing.
  base::Time old_enough = base::Time::Now() - base::TimeDelta::FromDays(8);
  SetInstallTime(kId1, old_enough, prefs);
  SetInstallTime(kId2, base::Time::Now(), prefs);
  SetInstallTime(kId3, old_enough, prefs);

  scoped_ptr<TestProxyOverriddenBubbleController> controller(
      new TestProxyOverriddenBubbleController(profile()));

  // The second extension is too new to warn about.
  EXPECT_FALSE(controller->ShouldShow(kId1));
  EXPECT_FALSE(controller->ShouldShow(kId2));
  // Lets make it old enough.
  SetInstallTime(kId2, old_enough, prefs);

  // The list will contain one enabled unpacked extension (ext 2).
  EXPECT_TRUE(controller->ShouldShow(kId2));
  EXPECT_FALSE(controller->ShouldShow(kId3));
  std::vector<base::string16> override_extensions =
      controller->GetExtensionList();
  ASSERT_EQ(1U, override_extensions.size());
  EXPECT_EQ(base::ASCIIToUTF16("Extension 2"), override_extensions[0]);
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
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
  // Only extension 2 should have been acknowledged.
  EXPECT_FALSE(prefs->HasProxyOverriddenBubbleBeenAcknowledged(kId1));
  EXPECT_TRUE(prefs->HasProxyOverriddenBubbleBeenAcknowledged(kId2));
  EXPECT_FALSE(prefs->HasProxyOverriddenBubbleBeenAcknowledged(kId3));
  // Clean up after ourselves.
  prefs->SetProxyOverriddenBubbleBeenAcknowledged(kId2, false);

  // Simulate clicking the learn more link to dismiss it.
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_LINK);
  controller.reset(new TestProxyOverriddenBubbleController(profile()));
  EXPECT_TRUE(controller->ShouldShow(kId2));
  controller->Show(&bubble);
  EXPECT_EQ(1U, controller->link_click_count());
  EXPECT_EQ(0U, controller->action_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  // No extension should have become disabled.
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
  // Only extension 2 should have been acknowledged.
  EXPECT_FALSE(prefs->HasProxyOverriddenBubbleBeenAcknowledged(kId1));
  EXPECT_TRUE(prefs->HasProxyOverriddenBubbleBeenAcknowledged(kId2));
  EXPECT_FALSE(prefs->HasProxyOverriddenBubbleBeenAcknowledged(kId3));
  // Clean up after ourselves.
  prefs->SetProxyOverriddenBubbleBeenAcknowledged(kId2, false);

  // Do it again, but now opt to disable the extension.
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_ACTION_BUTTON);
  controller.reset(new TestProxyOverriddenBubbleController(profile()));
  EXPECT_TRUE(controller->ShouldShow(kId2));
  override_extensions = controller->GetExtensionList();
  EXPECT_EQ(1U, override_extensions.size());
  controller->Show(&bubble);  // Simulate showing the bubble.
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(1U, controller->action_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  // Only extension 2 should have become disabled.
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
  EXPECT_TRUE(registry->disabled_extensions().GetByID(kId2) != NULL);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
  // No extension should have been acknowledged (it got disabled).
  EXPECT_FALSE(prefs->HasProxyOverriddenBubbleBeenAcknowledged(kId1));
  EXPECT_FALSE(prefs->HasProxyOverriddenBubbleBeenAcknowledged(kId2));
  EXPECT_FALSE(prefs->HasProxyOverriddenBubbleBeenAcknowledged(kId3));

  // Clean up after ourselves.
  service_->UninstallExtension(
      kId1, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
  service_->UninstallExtension(
      kId2, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
  service_->UninstallExtension(
      kId3, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
}

}  // namespace extensions
