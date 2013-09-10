// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/simple_test_clock.h"
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

  // A wrapper function for CheckReadFilteredData, so that we don't need to
  // enter empty string values for parameters we don't care about.
  void CheckReadData(
      ActivityLogPolicy* policy,
      const std::string& extension_id,
      int day,
      const base::Callback<void(scoped_ptr<Action::ActionVector>)>& checker) {
    CheckReadFilteredData(
        policy, extension_id, Action::ACTION_ANY, "", "", "", day, checker);
  }

  // A helper function to call ReadFilteredData on a policy object and wait for
  // the results to be processed.
  void CheckReadFilteredData(
      ActivityLogPolicy* policy,
      const std::string& extension_id,
      const Action::ActionType type,
      const std::string& api_name,
      const std::string& page_url,
      const std::string& arg_url,
      const int days_ago,
      const base::Callback<void(scoped_ptr<Action::ActionVector>)>& checker) {
    // Submit a request to the policy to read back some data, and call the
    // checker function when results are available.  This will happen on the
    // database thread.
    policy->ReadFilteredData(
        extension_id,
        type,
        api_name,
        page_url,
        arg_url,
        days_ago,
        base::Bind(&FullStreamUIPolicyTest::CheckWrapper,
                   checker,
                   base::MessageLoop::current()->QuitClosure()));

    // Set up a timeout that will trigger after 8 seconds; if we haven't
    // received any results by then assume that the test is broken.
    base::CancelableClosure timeout(
        base::Bind(&FullStreamUIPolicyTest::TimeoutCallback));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, timeout.callback(), base::TimeDelta::FromSeconds(8));

    // Wait for results; either the checker or the timeout callbacks should
    // cause the main loop to exit.
    base::MessageLoop::current()->Run();

    timeout.Cancel();
  }

  static void CheckWrapper(
      const base::Callback<void(scoped_ptr<Action::ActionVector>)>& checker,
      const base::Closure& done,
      scoped_ptr<Action::ActionVector> results) {
    checker.Run(results.Pass());
    done.Run();
  }

  static void TimeoutCallback() {
    base::MessageLoop::current()->QuitWhenIdle();
    FAIL() << "Policy test timed out waiting for results";
  }

  static void RetrieveActions_LogAndFetchActions(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
  }

  static void RetrieveActions_FetchFilteredActions0(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(0, static_cast<int>(i->size()));
  }

  static void RetrieveActions_FetchFilteredActions1(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(1, static_cast<int>(i->size()));
  }

  static void RetrieveActions_FetchFilteredActions2(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
  }

  static void RetrieveActions_FetchFilteredActions300(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(300, static_cast<int>(i->size()));
  }

  static void Arguments_Present(scoped_ptr<Action::ActionVector> i) {
    scoped_refptr<Action> last = i->front();
    CheckAction(*last, "odlameecjipmbmbejkplpemijjgpljce",
                Action::ACTION_API_CALL, "extension.connect",
                "[\"hello\",\"world\"]", "", "", "");
  }

  static void Arguments_GetTodaysActions(
      scoped_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "http://www.google.com/", "Page Title",
                "http://www.arg-url.com/");
    CheckAction(*actions->at(1), "punky", Action::ACTION_API_CALL, "brewster",
                "[\"woof\"]", "", "Page Title", "http://www.arg-url.com/");
  }

  static void Arguments_GetOlderActions(
      scoped_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "http://www.google.com/", "", "");
    CheckAction(*actions->at(1), "punky", Action::ACTION_API_CALL, "brewster",
                "[\"woof\"]", "", "", "");
  }

  static void AllURLsRemoved(scoped_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_API_CALL, "lets",
                "[\"vamoose\"]", "", "", "");
    CheckAction(*actions->at(1), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "", "", "");
  }

  static void SomeURLsRemoved(scoped_ptr<Action::ActionVector> actions) {
    // These will be in the vector in reverse time order.
    ASSERT_EQ(5, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "http://www.google.com/", "Google",
                "http://www.args-url.com/");
    CheckAction(*actions->at(1), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "http://www.google.com/", "Google", "");
    CheckAction(*actions->at(2), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "", "", "");
    CheckAction(*actions->at(3), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "", "", "http://www.google.com/");
    CheckAction(*actions->at(4), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "", "", "");
  }

  static void CheckAction(const Action& action,
                          const std::string& expected_id,
                          const Action::ActionType& expected_type,
                          const std::string& expected_api_name,
                          const std::string& expected_args_str,
                          const std::string& expected_page_url,
                          const std::string& expected_page_title,
                          const std::string& expected_arg_url) {
    ASSERT_EQ(expected_id, action.extension_id());
    ASSERT_EQ(expected_type, action.action_type());
    ASSERT_EQ(expected_api_name, action.api_name());
    ASSERT_EQ(expected_args_str,
              ActivityLogPolicy::Util::Serialize(action.args()));
    ASSERT_EQ(expected_page_url, action.SerializePageUrl());
    ASSERT_EQ(expected_page_title, action.page_title());
    ASSERT_EQ(expected_arg_url, action.SerializeArgUrl());
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
  ActivityLogPolicy* policy = new FullStreamUIPolicy(profile_.get());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension.get());
  scoped_ptr<base::ListValue> args(new base::ListValue());
  scoped_refptr<Action> action = new Action(extension->id(),
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "tabs.testMethod");
  action->set_args(args.Pass());
  policy->ProcessAction(action);
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, LogAndFetchActions) {
  ActivityLogPolicy* policy = new FullStreamUIPolicy(profile_.get());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension.get());
  GURL gurl("http://www.google.com");

  // Write some API calls
  scoped_refptr<Action> action_api = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_API_CALL,
                                                "tabs.testMethod");
  action_api->set_args(make_scoped_ptr(new base::ListValue()));
  policy->ProcessAction(action_api);

  scoped_refptr<Action> action_dom = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_DOM_ACCESS,
                                                "document.write");
  action_dom->set_args(make_scoped_ptr(new base::ListValue()));
  action_dom->set_page_url(gurl);
  policy->ProcessAction(action_dom);

  CheckReadData(
      policy,
      extension->id(),
      0,
      base::Bind(&FullStreamUIPolicyTest::RetrieveActions_LogAndFetchActions));

  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, LogAndFetchFilteredActions) {
  ActivityLogPolicy* policy = new FullStreamUIPolicy(profile_.get());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension.get());
  GURL gurl("http://www.google.com");

  // Write some API calls
  scoped_refptr<Action> action_api = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_API_CALL,
                                                "tabs.testMethod");
  action_api->set_args(make_scoped_ptr(new base::ListValue()));
  policy->ProcessAction(action_api);

  scoped_refptr<Action> action_dom = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_DOM_ACCESS,
                                                "document.write");
  action_dom->set_args(make_scoped_ptr(new base::ListValue()));
  action_dom->set_page_url(gurl);
  policy->ProcessAction(action_dom);

  CheckReadFilteredData(
      policy,
      extension->id(),
      Action::ACTION_API_CALL,
      "tabs.testMethod",
      "",
      "",
      -1,
      base::Bind(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy,
      "",
      Action::ACTION_DOM_ACCESS,
      "",
      "",
      "",
      -1,
      base::Bind(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy,
      "",
      Action::ACTION_DOM_ACCESS,
      "",
      "http://www.google.com/",
      "",
      -1,
      base::Bind(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy,
      "",
      Action::ACTION_DOM_ACCESS,
      "",
      "http://www.google.com",
      "",
      -1,
      base::Bind(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy,
      "",
      Action::ACTION_DOM_ACCESS,
      "",
      "http://www.goo",
      "",
      -1,
      base::Bind(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy,
      extension->id(),
      Action::ACTION_ANY,
      "",
      "",
      "",
      -1,
      base::Bind(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions2));

  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, LogWithArguments) {
  ActivityLogPolicy* policy = new FullStreamUIPolicy(profile_.get());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension.get());

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Set(0, new base::StringValue("hello"));
  args->Set(1, new base::StringValue("world"));
  scoped_refptr<Action> action = new Action(extension->id(),
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "extension.connect");
  action->set_args(args.Pass());

  policy->ProcessAction(action);
  CheckReadData(policy,
                extension->id(),
                0,
                base::Bind(&FullStreamUIPolicyTest::Arguments_Present));
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, GetTodaysActions) {
  ActivityLogPolicy* policy = new FullStreamUIPolicy(profile_.get());

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.  Note: Ownership is passed
  // to the policy, but we still keep a pointer locally.  The policy will take
  // care of destruction; this is safe since the policy outlives all our
  // accesses to the mock clock.
  base::SimpleTestClock* mock_clock = new base::SimpleTestClock();
  mock_clock->SetNow(base::Time::Now().LocalMidnight() +
                     base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(scoped_ptr<base::Clock>(mock_clock));

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky",
                 mock_clock->Now() - base::TimeDelta::FromMinutes(40),
                 Action::ACTION_API_CALL,
                 "brewster");
  action->mutable_args()->AppendString("woof");
  action->set_arg_url(GURL("http://www.arg-url.com"));
  action->set_page_title("Page Title");
  policy->ProcessAction(action);

  action =
      new Action("punky", mock_clock->Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_arg_url(GURL("http://www.arg-url.com"));
  action->set_page_title("Page Title");
  policy->ProcessAction(action);

  action = new Action(
      "scoobydoo", mock_clock->Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_arg_url(GURL("http://www.arg-url.com"));
  policy->ProcessAction(action);

  CheckReadData(
      policy,
      "punky",
      0,
      base::Bind(&FullStreamUIPolicyTest::Arguments_GetTodaysActions));
  policy->Close();
}

// Check that we can read back less recent actions in the db.
TEST_F(FullStreamUIPolicyTest, GetOlderActions) {
  ActivityLogPolicy* policy = new FullStreamUIPolicy(profile_.get());

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock* mock_clock = new base::SimpleTestClock();
  mock_clock->SetNow(base::Time::Now().LocalMidnight() +
                     base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(scoped_ptr<base::Clock>(mock_clock));

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky",
                 mock_clock->Now() - base::TimeDelta::FromDays(3) -
                     base::TimeDelta::FromMinutes(40),
                 Action::ACTION_API_CALL,
                 "brewster");
  action->mutable_args()->AppendString("woof");
  policy->ProcessAction(action);

  action = new Action("punky",
                      mock_clock->Now() - base::TimeDelta::FromDays(3),
                      Action::ACTION_DOM_ACCESS,
                      "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action = new Action("punky",
                      mock_clock->Now(),
                      Action::ACTION_DOM_ACCESS,
                      "lets");
  action->mutable_args()->AppendString("too new");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action = new Action("punky",
                      mock_clock->Now() - base::TimeDelta::FromDays(7),
                      Action::ACTION_DOM_ACCESS,
                      "lets");
  action->mutable_args()->AppendString("too old");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  CheckReadData(
      policy,
      "punky",
      3,
      base::Bind(&FullStreamUIPolicyTest::Arguments_GetOlderActions));
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, RemoveAllURLs) {
  ActivityLogPolicy* policy = new FullStreamUIPolicy(profile_.get());

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock* mock_clock = new base::SimpleTestClock();
  mock_clock->SetNow(base::Time::Now().LocalMidnight() +
                     base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(scoped_ptr<base::Clock>(mock_clock));

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky", mock_clock->Now(),
                 Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  mock_clock->Advance(base::TimeDelta::FromSeconds(1));
  action = new Action(
      "punky", mock_clock->Now(), Action::ACTION_API_CALL, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google2.com"));
  action->set_page_title("Google");
  // Deliberately no arg url set to make sure it still works when there is no
  // arg url.
  policy->ProcessAction(action);

  // Clean all the URLs.
  std::vector<GURL> no_url_restrictions;
  policy->RemoveURLs(no_url_restrictions);

  CheckReadData(
      policy,
      "punky",
      0,
      base::Bind(&FullStreamUIPolicyTest::AllURLsRemoved));
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, RemoveSpecificURLs) {
  ActivityLogPolicy* policy = new FullStreamUIPolicy(profile_.get());

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock* mock_clock = new base::SimpleTestClock();
  mock_clock->SetNow(base::Time::Now().LocalMidnight() +
                     base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(scoped_ptr<base::Clock>(mock_clock));

  // Record some actions
  // This should have the page url and args url cleared.
  scoped_refptr<Action> action = new Action("punky", mock_clock->Now(),
                                            Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google1.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google1.com"));
  policy->ProcessAction(action);

  // This should have the page url cleared but not args url.
  mock_clock->Advance(base::TimeDelta::FromSeconds(1));
  action = new Action(
      "punky", mock_clock->Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google1.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  // This should have the page url cleared. The args url is deliberately not set
  // to make sure this doesn't cause any issues.
  mock_clock->Advance(base::TimeDelta::FromSeconds(1));
  action = new Action(
      "punky", mock_clock->Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google2.com"));
  action->set_page_title("Google");
  policy->ProcessAction(action);

  // This should have the args url cleared but not the page url or page title.
  mock_clock->Advance(base::TimeDelta::FromSeconds(1));
  action = new Action(
      "punky", mock_clock->Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google1.com"));
  policy->ProcessAction(action);

  // This should have neither cleared.
  mock_clock->Advance(base::TimeDelta::FromSeconds(1));
  action = new Action(
      "punky", mock_clock->Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.args-url.com"));
  policy->ProcessAction(action);

  // Clean some URLs.
  std::vector<GURL> urls;
  urls.push_back(GURL("http://www.google1.com"));
  urls.push_back(GURL("http://www.google2.com"));
  urls.push_back(GURL("http://www.url_not_in_db.com"));
  policy->RemoveURLs(urls);

  CheckReadData(
      policy,
      "punky",
      0,
      base::Bind(&FullStreamUIPolicyTest::SomeURLsRemoved));
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, CapReturns) {
  FullStreamUIPolicy* policy = new FullStreamUIPolicy(profile_.get());

  for (int i = 0; i < 305; i++) {
    scoped_refptr<Action> action =
        new Action("punky",
                   base::Time::Now(),
                   Action::ACTION_API_CALL,
                   base::StringPrintf("apicall_%d", i));
    policy->ProcessAction(action);
  }

  policy->Flush();
  BrowserThread::PostTaskAndReply(
      BrowserThread::DB,
      FROM_HERE,
      base::Bind(&base::DoNothing),
      base::MessageLoop::current()->QuitClosure());
  base::MessageLoop::current()->Run();

  CheckReadFilteredData(
      policy,
      "punky",
      Action::ACTION_ANY,
      "",
      "",
      "",
      -1,
      base::Bind(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions300));
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, DeleteActions) {
  ActivityLogPolicy* policy = new FullStreamUIPolicy(profile_.get());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension.get());
  GURL gurl("http://www.google.com");

  // Write some API calls.
  scoped_refptr<Action> action_api = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_API_CALL,
                                                "tabs.testMethod");
  action_api->set_args(make_scoped_ptr(new base::ListValue()));
  policy->ProcessAction(action_api);

  scoped_refptr<Action> action_dom = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_DOM_ACCESS,
                                                "document.write");
  action_dom->set_args(make_scoped_ptr(new base::ListValue()));
  action_dom->set_page_url(gurl);
  policy->ProcessAction(action_dom);

  CheckReadData(
      policy,
      extension->id(),
      0,
      base::Bind(&FullStreamUIPolicyTest::RetrieveActions_LogAndFetchActions));

  // Now delete them.
  policy->DeleteDatabase();

  CheckReadFilteredData(
      policy,
      "",
      Action::ACTION_ANY,
      "",
      "",
      "",
      -1,
      base::Bind(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions0));

  policy->Close();
}

}  // namespace extensions
