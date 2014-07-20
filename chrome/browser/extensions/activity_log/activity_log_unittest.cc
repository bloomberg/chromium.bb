// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/extension_builder.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace {

const char kExtensionId[] = "abc";

const char* kUrlApiCalls[] = {
    "HTMLButtonElement.formAction", "HTMLEmbedElement.src",
    "HTMLFormElement.action",       "HTMLFrameElement.src",
    "HTMLHtmlElement.manifest",     "HTMLIFrameElement.src",
    "HTMLImageElement.longDesc",    "HTMLImageElement.src",
    "HTMLImageElement.lowsrc",      "HTMLInputElement.formAction",
    "HTMLInputElement.src",         "HTMLLinkElement.href",
    "HTMLMediaElement.src",         "HTMLMediaElement.currentSrc",
    "HTMLModElement.cite",          "HTMLObjectElement.data",
    "HTMLQuoteElement.cite",        "HTMLScriptElement.src",
    "HTMLSourceElement.src",        "HTMLTrackElement.src",
    "HTMLVideoElement.poster"};

}  // namespace

namespace extensions {

class ActivityLogTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined OS_CHROMEOS
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
    CommandLine command_line(CommandLine::NO_PROGRAM);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogging);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogTesting);
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile()))->CreateExtensionService
            (&command_line, base::FilePath(), false);
    base::RunLoop().RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
#if defined OS_CHROMEOS
    test_user_manager_.reset();
#endif
    base::RunLoop().RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  static void RetrieveActions_LogAndFetchActions0(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(0, static_cast<int>(i->size()));
  }

  static void RetrieveActions_LogAndFetchActions2(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
  }

  void SetPolicy(bool log_arguments) {
    ActivityLog* activity_log = ActivityLog::GetInstance(profile());
    if (log_arguments)
      activity_log->SetDatabasePolicy(ActivityLogPolicy::POLICY_FULLSTREAM);
    else
      activity_log->SetDatabasePolicy(ActivityLogPolicy::POLICY_COUNTS);
  }

  bool GetDatabaseEnabled() {
    ActivityLog* activity_log = ActivityLog::GetInstance(profile());
    return activity_log->IsDatabaseEnabled();
  }

  bool GetWatchdogActive() {
    ActivityLog* activity_log = ActivityLog::GetInstance(profile());
    return activity_log->IsWatchdogAppActive();
  }

  static void Arguments_Prerender(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(1U, i->size());
    scoped_refptr<Action> last = i->front();

    ASSERT_EQ("odlameecjipmbmbejkplpemijjgpljce", last->extension_id());
    ASSERT_EQ(Action::ACTION_CONTENT_SCRIPT, last->action_type());
    ASSERT_EQ("[\"script\"]",
              ActivityLogPolicy::Util::Serialize(last->args()));
    ASSERT_EQ("http://www.google.com/", last->SerializePageUrl());
    ASSERT_EQ("{\"prerender\":true}",
              ActivityLogPolicy::Util::Serialize(last->other()));
    ASSERT_EQ("", last->api_name());
    ASSERT_EQ("", last->page_title());
    ASSERT_EQ("", last->SerializeArgUrl());
  }

  static void RetrieveActions_ArgUrlExtraction(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    const base::DictionaryValue* other = NULL;
    int dom_verb = -1;

    ASSERT_EQ(4U, i->size());
    scoped_refptr<Action> action = i->at(0);
    ASSERT_EQ("XMLHttpRequest.open", action->api_name());
    ASSERT_EQ("[\"POST\",\"\\u003Carg_url>\"]",
              ActivityLogPolicy::Util::Serialize(action->args()));
    ASSERT_EQ("http://api.google.com/", action->arg_url().spec());
    // Test that the dom_verb field was changed to XHR (from METHOD).  This
    // could be tested on all retrieved XHR actions but it would be redundant,
    // so just test once.
    other = action->other();
    ASSERT_TRUE(other);
    ASSERT_TRUE(other->GetInteger(activity_log_constants::kActionDomVerb,
                                  &dom_verb));
    ASSERT_EQ(DomActionType::XHR, dom_verb);

    action = i->at(1);
    ASSERT_EQ("XMLHttpRequest.open", action->api_name());
    ASSERT_EQ("[\"POST\",\"\\u003Carg_url>\"]",
              ActivityLogPolicy::Util::Serialize(action->args()));
    ASSERT_EQ("http://www.google.com/api/", action->arg_url().spec());

    action = i->at(2);
    ASSERT_EQ("XMLHttpRequest.open", action->api_name());
    ASSERT_EQ("[\"POST\",\"/api/\"]",
              ActivityLogPolicy::Util::Serialize(action->args()));
    ASSERT_FALSE(action->arg_url().is_valid());

    action = i->at(3);
    ASSERT_EQ("windows.create", action->api_name());
    ASSERT_EQ("[{\"url\":\"\\u003Carg_url>\"}]",
              ActivityLogPolicy::Util::Serialize(action->args()));
    ASSERT_EQ("http://www.google.co.uk/", action->arg_url().spec());
  }

  static void RetrieveActions_ArgUrlApiCalls(
      scoped_ptr<std::vector<scoped_refptr<Action> > > actions) {
    size_t api_calls_size = arraysize(kUrlApiCalls);
    const base::DictionaryValue* other = NULL;
    int dom_verb = -1;

    ASSERT_EQ(api_calls_size, actions->size());

    for (size_t i = 0; i < actions->size(); i++) {
      scoped_refptr<Action> action = actions->at(i);
      ASSERT_EQ(kExtensionId, action->extension_id());
      ASSERT_EQ(Action::ACTION_DOM_ACCESS, action->action_type());
      ASSERT_EQ(kUrlApiCalls[i], action->api_name());
      ASSERT_EQ("[\"\\u003Carg_url>\"]",
                ActivityLogPolicy::Util::Serialize(action->args()));
      ASSERT_EQ("http://www.google.co.uk/", action->arg_url().spec());
      other = action->other();
      ASSERT_TRUE(other);
      ASSERT_TRUE(
          other->GetInteger(activity_log_constants::kActionDomVerb, &dom_verb));
      ASSERT_EQ(DomActionType::SETTER, dom_verb);
    }
  }

  ExtensionService* extension_service_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif
};

TEST_F(ActivityLogTest, Construct) {
  ASSERT_TRUE(GetDatabaseEnabled());
  ASSERT_FALSE(GetWatchdogActive());
}

TEST_F(ActivityLogTest, LogAndFetchActions) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  scoped_ptr<base::ListValue> args(new base::ListValue());
  ASSERT_TRUE(GetDatabaseEnabled());

  // Write some API calls
  scoped_refptr<Action> action = new Action(kExtensionId,
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "tabs.testMethod");
  activity_log->LogAction(action);
  action = new Action(kExtensionId,
                      base::Time::Now(),
                      Action::ACTION_DOM_ACCESS,
                      "document.write");
  action->set_page_url(GURL("http://www.google.com"));
  activity_log->LogAction(action);

  activity_log->GetFilteredActions(
      kExtensionId,
      Action::ACTION_ANY,
      "",
      "",
      "",
      0,
      base::Bind(ActivityLogTest::RetrieveActions_LogAndFetchActions2));
}

TEST_F(ActivityLogTest, LogPrerender) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension.get());
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  ASSERT_TRUE(GetDatabaseEnabled());
  GURL url("http://www.google.com");

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()));

  prerender_manager->OnCookieStoreLoaded();

  const gfx::Size kSize(640, 480);
  scoped_ptr<prerender::PrerenderHandle> prerender_handle(
      prerender_manager->AddPrerenderFromLocalPredictor(
          url,
          web_contents()->GetController().GetDefaultSessionStorageNamespace(),
          kSize));

  const std::vector<content::WebContents*> contentses =
      prerender_manager->GetAllPrerenderingContents();
  ASSERT_EQ(1U, contentses.size());
  content::WebContents *contents = contentses[0];
  ASSERT_TRUE(prerender_manager->IsWebContentsPrerendering(contents, NULL));

  TabHelper::ScriptExecutionObserver::ExecutingScriptsMap executing_scripts;
  executing_scripts[extension->id()].insert("script");

  static_cast<TabHelper::ScriptExecutionObserver*>(activity_log)->
      OnScriptsExecuted(contents, executing_scripts, url);

  activity_log->GetFilteredActions(
      extension->id(),
      Action::ACTION_ANY,
      "",
      "",
      "",
      0,
      base::Bind(ActivityLogTest::Arguments_Prerender));

  prerender_manager->CancelAllPrerenders();
}

TEST_F(ActivityLogTest, ArgUrlExtraction) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  scoped_ptr<base::ListValue> args(new base::ListValue());

  base::Time now = base::Time::Now();

  // Submit a DOM API call which should have its URL extracted into the arg_url
  // field.
  scoped_refptr<Action> action = new Action(kExtensionId,
                                            now,
                                            Action::ACTION_DOM_ACCESS,
                                            "XMLHttpRequest.open");
  action->set_page_url(GURL("http://www.google.com/"));
  action->mutable_args()->AppendString("POST");
  action->mutable_args()->AppendString("http://api.google.com/");
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      DomActionType::METHOD);
  activity_log->LogAction(action);

  // Submit a DOM API call with a relative URL in the argument, which should be
  // resolved relative to the page URL.
  action = new Action(kExtensionId,
                      now - base::TimeDelta::FromSeconds(1),
                      Action::ACTION_DOM_ACCESS,
                      "XMLHttpRequest.open");
  action->set_page_url(GURL("http://www.google.com/"));
  action->mutable_args()->AppendString("POST");
  action->mutable_args()->AppendString("/api/");
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      DomActionType::METHOD);
  activity_log->LogAction(action);

  // Submit a DOM API call with a relative URL but no base page URL against
  // which to resolve.
  action = new Action(kExtensionId,
                      now - base::TimeDelta::FromSeconds(2),
                      Action::ACTION_DOM_ACCESS,
                      "XMLHttpRequest.open");
  action->mutable_args()->AppendString("POST");
  action->mutable_args()->AppendString("/api/");
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      DomActionType::METHOD);
  activity_log->LogAction(action);

  // Submit an API call with an embedded URL.
  action = new Action(kExtensionId,
                      now - base::TimeDelta::FromSeconds(3),
                      Action::ACTION_API_CALL,
                      "windows.create");
  action->set_args(
      ListBuilder()
          .Append(DictionaryBuilder().Set("url", "http://www.google.co.uk"))
          .Build());
  activity_log->LogAction(action);

  activity_log->GetFilteredActions(
      kExtensionId,
      Action::ACTION_ANY,
      "",
      "",
      "",
      -1,
      base::Bind(ActivityLogTest::RetrieveActions_ArgUrlExtraction));
}

TEST_F(ActivityLogTest, UninstalledExtension) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();

  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  scoped_ptr<base::ListValue> args(new base::ListValue());
  ASSERT_TRUE(GetDatabaseEnabled());

  // Write some API calls
  scoped_refptr<Action> action = new Action(extension->id(),
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "tabs.testMethod");
  activity_log->LogAction(action);
  action = new Action(extension->id(),
                      base::Time::Now(),
                      Action::ACTION_DOM_ACCESS,
                      "document.write");
  action->set_page_url(GURL("http://www.google.com"));

  activity_log->OnExtensionUninstalled(
      NULL, extension, extensions::UNINSTALL_REASON_FOR_TESTING);
  activity_log->GetFilteredActions(
      extension->id(),
      Action::ACTION_ANY,
      "",
      "",
      "",
      -1,
      base::Bind(ActivityLogTest::RetrieveActions_LogAndFetchActions0));
}

TEST_F(ActivityLogTest, ArgUrlApiCalls) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  scoped_ptr<base::ListValue> args(new base::ListValue());
  base::Time now = base::Time::Now();
  int api_calls_size = arraysize(kUrlApiCalls);
  scoped_refptr<Action> action;

  for (int i = 0; i < api_calls_size; i++) {
    action = new Action(kExtensionId,
                        now - base::TimeDelta::FromSeconds(i),
                        Action::ACTION_DOM_ACCESS,
                        kUrlApiCalls[i]);
    action->mutable_args()->AppendString("http://www.google.co.uk");
    action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                        DomActionType::SETTER);
    activity_log->LogAction(action);
  }

  activity_log->GetFilteredActions(
      kExtensionId,
      Action::ACTION_ANY,
      "",
      "",
      "",
      -1,
      base::Bind(ActivityLogTest::RetrieveActions_ArgUrlApiCalls));
}

}  // namespace extensions
