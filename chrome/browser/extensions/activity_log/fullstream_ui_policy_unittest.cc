// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace extensions {

class FullStreamUIPolicyTest : public testing::Test {
 public:
  FullStreamUIPolicyTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        saved_cmdline_(CommandLine::NO_PROGRAM) {
#if defined OS_CHROMEOS
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
    CommandLine command_line(CommandLine::NO_PROGRAM);
    saved_cmdline_ = *CommandLine::ForCurrentProcess();
    profile_.reset(new TestingProfile());
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogging);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogTesting);
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile_.get()))->CreateExtensionService
            (&command_line, base::FilePath(), false);
  }

  virtual ~FullStreamUIPolicyTest() {
#if defined OS_CHROMEOS
    test_user_manager_.reset();
#endif
    base::RunLoop().RunUntilIdle();
    profile_.reset(NULL);
    base::RunLoop().RunUntilIdle();
    // Restore the original command line and undo the affects of SetUp().
    *CommandLine::ForCurrentProcess() = saved_cmdline_;
  }

  static void RetrieveActions_LogAndFetchActions(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
  }

  static void Arguments_Present(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    scoped_refptr<Action> last = i->front();
    std::string args = "ID: odlameecjipmbmbejkplpemijjgpljce, CATEGORY: "
      "call, API: extension.connect, ARGS: \"hello\", \"world\"";
    ASSERT_EQ(args, last->PrintForDebug());
  }

 protected:
  ExtensionService* extension_service_;
  scoped_ptr<TestingProfile> profile_;
  content::TestBrowserThreadBundle thread_bundle_;
  // Used to preserve a copy of the original command line.
  // The test framework will do this itself as well. However, by then,
  // it is too late to call ActivityLog::RecomputeLoggingIsEnabled() in
  // TearDown().
  CommandLine saved_cmdline_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif
};

TEST_F(FullStreamUIPolicyTest, Construct) {
  scoped_ptr<ActivityLogPolicy> policy(new FullStreamUIPolicy(profile_.get()));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension);
  scoped_ptr<base::ListValue> args(new base::ListValue());
  policy->ProcessAction(ActivityLogPolicy::ACTION_API, extension->id(),
      std::string("tabs.testMethod"), GURL(), args.get(), NULL);
}

TEST_F(FullStreamUIPolicyTest, LogAndFetchActions) {
  scoped_ptr<ActivityLogPolicy> policy(new FullStreamUIPolicy(profile_.get()));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension);
  scoped_ptr<base::ListValue> args(new base::ListValue());
  GURL gurl("http://www.google.com");

  // Write some API calls
  policy->ProcessAction(ActivityLogPolicy::ACTION_API, extension->id(),
      std::string("tabs.testMethod"), GURL(), args.get(), NULL);
  policy->ProcessAction(ActivityLogPolicy::ACTION_DOM,
                        extension->id(), std::string("document.write"),
                        gurl, args.get(), NULL);
  policy->ReadData(extension->id(), 0,
      base::Bind(FullStreamUIPolicyTest::RetrieveActions_LogAndFetchActions));
}

TEST_F(FullStreamUIPolicyTest, LogWithArguments) {
  scoped_ptr<ActivityLogPolicy> policy(new FullStreamUIPolicy(profile_.get()));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension);
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Set(0, new base::StringValue("hello"));
  args->Set(1, new base::StringValue("world"));
  policy->ProcessAction(ActivityLogPolicy::ACTION_API, extension->id(),
      std::string("extension.connect"), GURL(), args.get(), NULL);
  policy->ReadData(extension->id(), 0,
      base::Bind(FullStreamUIPolicyTest::Arguments_Present));
}

}  // namespace extensions
