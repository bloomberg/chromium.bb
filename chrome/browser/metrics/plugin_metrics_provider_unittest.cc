// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/plugin_metrics_provider.h"

#include <string>

#include "base/basictypes.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

content::WebPluginInfo CreateFakePluginInfo(
    const std::string& name,
    const base::FilePath::CharType* path,
    const std::string& version,
    bool is_pepper) {
  content::WebPluginInfo plugin(base::UTF8ToUTF16(name),
                                base::FilePath(path),
                                base::UTF8ToUTF16(version),
                                base::string16());
  if (is_pepper)
    plugin.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
  else
    plugin.type = content::WebPluginInfo::PLUGIN_TYPE_NPAPI;
  return plugin;
}

}  // namespace

TEST(PluginMetricsProviderTest, IsPluginProcess) {
  EXPECT_TRUE(PluginMetricsProvider::IsPluginProcess(
      content::PROCESS_TYPE_PLUGIN));
  EXPECT_TRUE(PluginMetricsProvider::IsPluginProcess(
      content::PROCESS_TYPE_PPAPI_PLUGIN));
  EXPECT_FALSE(PluginMetricsProvider::IsPluginProcess(
      content::PROCESS_TYPE_GPU));
}

TEST(PluginMetricsProviderTest, Plugins) {
  content::TestBrowserThreadBundle thread_bundle;

  TestingPrefServiceSimple prefs;
  PluginMetricsProvider::RegisterPrefs(prefs.registry());
  PluginMetricsProvider provider(&prefs);

  std::vector<content::WebPluginInfo> plugins;
  plugins.push_back(CreateFakePluginInfo("p1", FILE_PATH_LITERAL("p1.plugin"),
                                         "1.5", true));
  plugins.push_back(CreateFakePluginInfo("p2", FILE_PATH_LITERAL("p2.plugin"),
                                         "2.0", false));
  provider.SetPluginsForTesting(plugins);

  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  ASSERT_EQ(2, system_profile.plugin_size());
  EXPECT_EQ("p1", system_profile.plugin(0).name());
  EXPECT_EQ("p1.plugin", system_profile.plugin(0).filename());
  EXPECT_EQ("1.5", system_profile.plugin(0).version());
  EXPECT_TRUE(system_profile.plugin(0).is_pepper());
  EXPECT_EQ("p2", system_profile.plugin(1).name());
  EXPECT_EQ("p2.plugin", system_profile.plugin(1).filename());
  EXPECT_EQ("2.0", system_profile.plugin(1).version());
  EXPECT_FALSE(system_profile.plugin(1).is_pepper());

  // Now set some plugin stability stats for p2 and verify they're recorded.
  scoped_ptr<base::DictionaryValue> plugin_dict(new base::DictionaryValue);
  plugin_dict->SetString(prefs::kStabilityPluginName, "p2");
  plugin_dict->SetInteger(prefs::kStabilityPluginLaunches, 1);
  plugin_dict->SetInteger(prefs::kStabilityPluginCrashes, 2);
  plugin_dict->SetInteger(prefs::kStabilityPluginInstances, 3);
  plugin_dict->SetInteger(prefs::kStabilityPluginLoadingErrors, 4);
  {
    ListPrefUpdate update(&prefs, prefs::kStabilityPluginStats);
    update.Get()->Append(plugin_dict.release());
  }

  provider.ProvideStabilityMetrics(&system_profile);

  const metrics::SystemProfileProto_Stability& stability =
      system_profile.stability();
  ASSERT_EQ(1, stability.plugin_stability_size());
  EXPECT_EQ("p2", stability.plugin_stability(0).plugin().name());
  EXPECT_EQ("p2.plugin", stability.plugin_stability(0).plugin().filename());
  EXPECT_EQ("2.0", stability.plugin_stability(0).plugin().version());
  EXPECT_FALSE(stability.plugin_stability(0).plugin().is_pepper());
  EXPECT_EQ(1, stability.plugin_stability(0).launch_count());
  EXPECT_EQ(2, stability.plugin_stability(0).crash_count());
  EXPECT_EQ(3, stability.plugin_stability(0).instance_count());
  EXPECT_EQ(4, stability.plugin_stability(0).loading_error_count());
}
