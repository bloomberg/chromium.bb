// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_info_message_filter.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/npapi/mock_plugin_list.h"

using content::PluginService;

namespace {

class FakePluginServiceFilter : public content::PluginServiceFilter {
 public:
  FakePluginServiceFilter() {}
  virtual ~FakePluginServiceFilter() {}

  virtual bool ShouldUsePlugin(int render_process_id,
                               int render_view_id,
                               const void* context,
                               const GURL& url,
                               const GURL& policy_url,
                               webkit::WebPluginInfo* plugin) OVERRIDE;

  void set_plugin_enabled(const FilePath& plugin_path, bool enabled) {
    plugin_state_[plugin_path] = enabled;
  }

 private:
  std::map<FilePath, bool> plugin_state_;
};

bool FakePluginServiceFilter::ShouldUsePlugin(int render_process_id,
                                              int render_view_id,
                                              const void* context,
                                              const GURL& url,
                                              const GURL& policy_url,
                                              webkit::WebPluginInfo* plugin) {
  std::map<FilePath, bool>::iterator it = plugin_state_.find(plugin->path);
  if (it == plugin_state_.end()) {
    ADD_FAILURE() << "No plug-in state for '" << plugin->path.value() << "'";
    return false;
  }
  return it->second;
}

}  // namespace

class PluginInfoMessageFilterTest : public ::testing::Test {
 public:
  PluginInfoMessageFilterTest() :
      foo_plugin_path_(FILE_PATH_LITERAL("/path/to/foo")),
      bar_plugin_path_(FILE_PATH_LITERAL("/path/to/bar")),
      file_thread_(content::BrowserThread::FILE, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    webkit::WebPluginInfo foo_plugin(ASCIIToUTF16("Foo Plug-in"),
                                     foo_plugin_path_,
                                     ASCIIToUTF16("1"),
                                     ASCIIToUTF16("The Foo plug-in."));
    webkit::WebPluginMimeType mimeType;
    mimeType.mime_type = "foo/bar";
    foo_plugin.mime_types.push_back(mimeType);
    plugin_list_.AddPluginToLoad(foo_plugin);

    webkit::WebPluginInfo bar_plugin(ASCIIToUTF16("Bar Plug-in"),
                                     bar_plugin_path_,
                                     ASCIIToUTF16("1"),
                                     ASCIIToUTF16("The Bar plug-in."));
    mimeType.mime_type = "foo/bar";
    bar_plugin.mime_types.push_back(mimeType);
    plugin_list_.AddPluginToLoad(bar_plugin);

    PluginService::GetInstance()->SetPluginListForTesting(&plugin_list_);
    PluginService::GetInstance()->SetFilter(&filter_);

    PluginService::GetInstance()->GetPlugins(
        base::Bind(&PluginInfoMessageFilterTest::PluginsLoaded,
                   base::Unretained(this)));
    base::RunLoop run_loop;
    run_loop.Run();
  }

 protected:
  FilePath foo_plugin_path_;
  FilePath bar_plugin_path_;
  FakePluginServiceFilter filter_;
  PluginInfoMessageFilter::Context context_;

 private:
  void PluginsLoaded(const std::vector<webkit::WebPluginInfo>& plugins) {
    MessageLoop::current()->Quit();
  }

  MessageLoop message_loop_;
  // PluginService::GetPlugins on Windows jumps to the FILE thread even with
  // a MockPluginList.
  content::TestBrowserThread file_thread_;
  base::ShadowingAtExitManager at_exit_manager_;  // Destroys the PluginService.
  webkit::npapi::MockPluginList plugin_list_;
};

TEST_F(PluginInfoMessageFilterTest, FindEnabledPlugin) {
  filter_.set_plugin_enabled(foo_plugin_path_, true);
  filter_.set_plugin_enabled(bar_plugin_path_, true);
  {
    ChromeViewHostMsg_GetPluginInfo_Status status;
    webkit::WebPluginInfo plugin;
    std::string actual_mime_type;
    EXPECT_TRUE(context_.FindEnabledPlugin(
        0, GURL(), GURL(), "foo/bar", &status, &plugin, &actual_mime_type,
        NULL));
    EXPECT_EQ(ChromeViewHostMsg_GetPluginInfo_Status::kAllowed, status.value);
    EXPECT_EQ(foo_plugin_path_.value(), plugin.path.value());
  }

  filter_.set_plugin_enabled(foo_plugin_path_, false);
  {
    ChromeViewHostMsg_GetPluginInfo_Status status;
    webkit::WebPluginInfo plugin;
    std::string actual_mime_type;
    EXPECT_TRUE(context_.FindEnabledPlugin(
        0, GURL(), GURL(), "foo/bar", &status, &plugin, &actual_mime_type,
        NULL));
    EXPECT_EQ(ChromeViewHostMsg_GetPluginInfo_Status::kAllowed, status.value);
    EXPECT_EQ(bar_plugin_path_.value(), plugin.path.value());
  }

  filter_.set_plugin_enabled(bar_plugin_path_, false);
  {
    ChromeViewHostMsg_GetPluginInfo_Status status;
    webkit::WebPluginInfo plugin;
    std::string actual_mime_type;
    std::string identifier;
    string16 plugin_name;
    EXPECT_FALSE(context_.FindEnabledPlugin(
        0, GURL(), GURL(), "foo/bar", &status, &plugin, &actual_mime_type,
        NULL));
    EXPECT_EQ(ChromeViewHostMsg_GetPluginInfo_Status::kDisabled, status.value);
    EXPECT_EQ(foo_plugin_path_.value(), plugin.path.value());
  }
  {
    ChromeViewHostMsg_GetPluginInfo_Status status;
    webkit::WebPluginInfo plugin;
    std::string actual_mime_type;
    EXPECT_FALSE(context_.FindEnabledPlugin(
        0, GURL(), GURL(), "baz/blurp", &status, &plugin, &actual_mime_type,
        NULL));
    EXPECT_EQ(ChromeViewHostMsg_GetPluginInfo_Status::kNotFound, status.value);
    EXPECT_EQ(FILE_PATH_LITERAL(""), plugin.path.value());
  }
}
