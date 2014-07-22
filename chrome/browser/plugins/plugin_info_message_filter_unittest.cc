// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_info_message_filter.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::PluginService;

namespace {

void PluginsLoaded(const base::Closure& callback,
                   const std::vector<content::WebPluginInfo>& plugins) {
  callback.Run();
}

class FakePluginServiceFilter : public content::PluginServiceFilter {
 public:
  FakePluginServiceFilter() {}
  virtual ~FakePluginServiceFilter() {}

  virtual bool IsPluginAvailable(int render_process_id,
                                 int render_view_id,
                                 const void* context,
                                 const GURL& url,
                                 const GURL& policy_url,
                                 content::WebPluginInfo* plugin) OVERRIDE;

  virtual bool CanLoadPlugin(int render_process_id,
                             const base::FilePath& path) OVERRIDE;

  void set_plugin_enabled(const base::FilePath& plugin_path, bool enabled) {
    plugin_state_[plugin_path] = enabled;
  }

 private:
  std::map<base::FilePath, bool> plugin_state_;
};

bool FakePluginServiceFilter::IsPluginAvailable(
    int render_process_id,
    int render_view_id,
    const void* context,
    const GURL& url,
    const GURL& policy_url,
    content::WebPluginInfo* plugin) {
  std::map<base::FilePath, bool>::iterator it =
      plugin_state_.find(plugin->path);
  if (it == plugin_state_.end()) {
    ADD_FAILURE() << "No plug-in state for '" << plugin->path.value() << "'";
    return false;
  }
  return it->second;
}

bool FakePluginServiceFilter::CanLoadPlugin(int render_process_id,
                                            const base::FilePath& path) {
  return true;
}

}  // namespace

class PluginInfoMessageFilterTest : public ::testing::Test {
 public:
  PluginInfoMessageFilterTest() :
      foo_plugin_path_(FILE_PATH_LITERAL("/path/to/foo")),
      bar_plugin_path_(FILE_PATH_LITERAL("/path/to/bar")),
      context_(0, &profile_) {
  }

  virtual void SetUp() OVERRIDE {
    content::WebPluginInfo foo_plugin(base::ASCIIToUTF16("Foo Plug-in"),
                                      foo_plugin_path_,
                                      base::ASCIIToUTF16("1"),
                                      base::ASCIIToUTF16("The Foo plug-in."));
    content::WebPluginMimeType mime_type;
    mime_type.mime_type = "foo/bar";
    foo_plugin.mime_types.push_back(mime_type);
    foo_plugin.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
    PluginService::GetInstance()->Init();
    PluginService::GetInstance()->RegisterInternalPlugin(foo_plugin, false);

    content::WebPluginInfo bar_plugin(base::ASCIIToUTF16("Bar Plug-in"),
                                      bar_plugin_path_,
                                      base::ASCIIToUTF16("1"),
                                      base::ASCIIToUTF16("The Bar plug-in."));
    mime_type.mime_type = "foo/bar";
    bar_plugin.mime_types.push_back(mime_type);
    bar_plugin.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
    PluginService::GetInstance()->RegisterInternalPlugin(bar_plugin, false);

    PluginService::GetInstance()->SetFilter(&filter_);

#if !defined(OS_WIN)
    // Can't go out of process in unit tests.
    content::RenderProcessHost::SetRunRendererInProcess(true);
#endif
    base::RunLoop run_loop;
    PluginService::GetInstance()->GetPlugins(
        base::Bind(&PluginsLoaded, run_loop.QuitClosure()));
    run_loop.Run();
#if !defined(OS_WIN)
    content::RenderProcessHost::SetRunRendererInProcess(false);
#endif
  }

 protected:
  TestingProfile* profile() {
    return &profile_;
  }

  PluginInfoMessageFilter::Context* context() {
    return &context_;
  }

  void VerifyPluginContentSetting(const GURL& url,
                                  const std::string& plugin,
                                  ContentSetting expected_setting,
                                  bool expected_is_default,
                                  bool expected_is_managed) {
    ContentSetting setting = expected_setting == CONTENT_SETTING_DEFAULT ?
        CONTENT_SETTING_BLOCK : CONTENT_SETTING_DEFAULT;
    bool is_default = !expected_is_default;
    bool is_managed = !expected_is_managed;
    context()->GetPluginContentSetting(
        content::WebPluginInfo(), url, url, plugin,
        &setting, &is_default, &is_managed);
    EXPECT_EQ(expected_setting, setting);
    EXPECT_EQ(expected_is_default, is_default);
    EXPECT_EQ(expected_is_managed, is_managed);
  }

  base::FilePath foo_plugin_path_;
  base::FilePath bar_plugin_path_;
  FakePluginServiceFilter filter_;

 private:
  base::ShadowingAtExitManager at_exit_manager_;  // Destroys the PluginService.
  content::TestBrowserThreadBundle test_thread_bundle;
  TestingProfile profile_;
  PluginInfoMessageFilter::Context context_;
};

TEST_F(PluginInfoMessageFilterTest, FindEnabledPlugin) {
  filter_.set_plugin_enabled(foo_plugin_path_, true);
  filter_.set_plugin_enabled(bar_plugin_path_, true);
  {
    ChromeViewHostMsg_GetPluginInfo_Status status;
    content::WebPluginInfo plugin;
    std::string actual_mime_type;
    EXPECT_TRUE(context()->FindEnabledPlugin(
        0, GURL(), GURL(), "foo/bar", &status, &plugin, &actual_mime_type,
        NULL));
    EXPECT_EQ(ChromeViewHostMsg_GetPluginInfo_Status::kAllowed, status.value);
    EXPECT_EQ(foo_plugin_path_.value(), plugin.path.value());
  }

  filter_.set_plugin_enabled(foo_plugin_path_, false);
  {
    ChromeViewHostMsg_GetPluginInfo_Status status;
    content::WebPluginInfo plugin;
    std::string actual_mime_type;
    EXPECT_TRUE(context()->FindEnabledPlugin(
        0, GURL(), GURL(), "foo/bar", &status, &plugin, &actual_mime_type,
        NULL));
    EXPECT_EQ(ChromeViewHostMsg_GetPluginInfo_Status::kAllowed, status.value);
    EXPECT_EQ(bar_plugin_path_.value(), plugin.path.value());
  }

  filter_.set_plugin_enabled(bar_plugin_path_, false);
  {
    ChromeViewHostMsg_GetPluginInfo_Status status;
    content::WebPluginInfo plugin;
    std::string actual_mime_type;
    std::string identifier;
    base::string16 plugin_name;
    EXPECT_FALSE(context()->FindEnabledPlugin(
        0, GURL(), GURL(), "foo/bar", &status, &plugin, &actual_mime_type,
        NULL));
    EXPECT_EQ(ChromeViewHostMsg_GetPluginInfo_Status::kDisabled, status.value);
    EXPECT_EQ(foo_plugin_path_.value(), plugin.path.value());
  }
  {
    ChromeViewHostMsg_GetPluginInfo_Status status;
    content::WebPluginInfo plugin;
    std::string actual_mime_type;
    EXPECT_FALSE(context()->FindEnabledPlugin(
        0, GURL(), GURL(), "baz/blurp", &status, &plugin, &actual_mime_type,
        NULL));
    EXPECT_EQ(ChromeViewHostMsg_GetPluginInfo_Status::kNotFound, status.value);
    EXPECT_EQ(FILE_PATH_LITERAL(""), plugin.path.value());
  }
}

TEST_F(PluginInfoMessageFilterTest, GetPluginContentSetting) {
  HostContentSettingsMap* map = profile()->GetHostContentSettingsMap();

  // Block plugins by default.
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_BLOCK);

  // Set plugins to click-to-play on example.com and subdomains.
  ContentSettingsPattern pattern =
       ContentSettingsPattern::FromString("[*.]example.com");
  map->SetContentSetting(pattern,
                         ContentSettingsPattern::Wildcard(),
                         CONTENT_SETTINGS_TYPE_PLUGINS,
                         std::string(),
                         CONTENT_SETTING_ASK);

  // Allow plugin "foo" on all sites.
  map->SetContentSetting(ContentSettingsPattern::Wildcard(),
                         ContentSettingsPattern::Wildcard(),
                         CONTENT_SETTINGS_TYPE_PLUGINS,
                         "foo",
                         CONTENT_SETTING_ALLOW);

  GURL unmatched_host("https://www.google.com");
  GURL host("http://example.com/");
  ASSERT_EQ(CONTENT_SETTING_BLOCK, map->GetContentSetting(
      unmatched_host, unmatched_host, CONTENT_SETTINGS_TYPE_PLUGINS,
      std::string()));
  ASSERT_EQ(CONTENT_SETTING_ASK, map->GetContentSetting(
      host, host, CONTENT_SETTINGS_TYPE_PLUGINS, std::string()));
  ASSERT_EQ(CONTENT_SETTING_ALLOW, map->GetContentSetting(
      host, host, CONTENT_SETTINGS_TYPE_PLUGINS, "foo"));
  ASSERT_EQ(CONTENT_SETTING_DEFAULT, map->GetContentSetting(
      host, host, CONTENT_SETTINGS_TYPE_PLUGINS, "bar"));

  // "foo" is allowed everywhere.
  VerifyPluginContentSetting(host, "foo", CONTENT_SETTING_ALLOW, false, false);

  // There is no specific content setting for "bar", so the general setting
  // for example.com applies.
  VerifyPluginContentSetting(host, "bar", CONTENT_SETTING_ASK, false, false);

  // Otherwise, use the default.
  VerifyPluginContentSetting(unmatched_host, "bar", CONTENT_SETTING_BLOCK,
                             true, false);

  // Block plugins via policy.
  TestingPrefServiceSyncable* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        new base::FundamentalValue(CONTENT_SETTING_BLOCK));

  // All plugins should be blocked now.
  VerifyPluginContentSetting(host, "foo", CONTENT_SETTING_BLOCK, true, true);
  VerifyPluginContentSetting(host, "bar", CONTENT_SETTING_BLOCK, true, true);
  VerifyPluginContentSetting(unmatched_host, "bar", CONTENT_SETTING_BLOCK,
                             true, true);
}
