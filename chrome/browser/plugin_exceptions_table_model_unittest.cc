// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/table_model_observer.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/plugin_exceptions_table_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/plugins/webplugininfo.h"

// Can't be an internal namespace because PluginExceptionsTableModel declares
// as a friend.
namespace plugin_test_internal {

class MockTableModelObserver : public TableModelObserver {
 public:
  MOCK_METHOD0(OnModelChanged, void());
  MOCK_METHOD2(OnItemsChanged, void(int start, int length));
  MOCK_METHOD2(OnItemsAdded, void(int start, int length));
  MOCK_METHOD2(OnItemsRemoved, void(int start, int length));
};

class TestingPluginExceptionsTableModel : public PluginExceptionsTableModel {
 public:
  TestingPluginExceptionsTableModel(HostContentSettingsMap* map,
                                    HostContentSettingsMap* otr_map)
      : PluginExceptionsTableModel(map, otr_map) {}
  virtual ~TestingPluginExceptionsTableModel() {}

  void set_plugins(const std::vector<WebPluginInfo>& plugins) {
    plugins_ = plugins;
  }

 protected:
  virtual void GetPlugins(std::vector<WebPluginInfo>* plugins) {
    *plugins = plugins_;
  }

 private:
  std::vector<WebPluginInfo> plugins_;
};

class PluginExceptionsTableModelTest : public testing::Test {
 public:
  PluginExceptionsTableModelTest()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        command_line_(*CommandLine::ForCurrentProcess()) {}

  virtual void SetUp() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableResourceContentSettings);

    profile_.reset(new TestingProfile());

    HostContentSettingsMap* map = profile_->GetHostContentSettingsMap();

    HostContentSettingsMap::Pattern example_com("[*.]example.com");
    HostContentSettingsMap::Pattern moose_org("[*.]moose.org");
    map->SetContentSetting(example_com,
                           CONTENT_SETTINGS_TYPE_PLUGINS,
                           "foo",
                           CONTENT_SETTING_ALLOW);
    map->SetContentSetting(moose_org,
                           CONTENT_SETTINGS_TYPE_PLUGINS,
                           "bar",
                           CONTENT_SETTING_BLOCK);
    map->SetContentSetting(example_com,
                           CONTENT_SETTINGS_TYPE_PLUGINS,
                           "bar",
                           CONTENT_SETTING_ALLOW);

    table_model_.reset(new TestingPluginExceptionsTableModel(map, NULL));

    std::vector<WebPluginInfo> plugins;
    WebPluginInfo foo_plugin;
    foo_plugin.path = FilePath(FILE_PATH_LITERAL("foo"));
    foo_plugin.name = ASCIIToUTF16("FooPlugin");
    foo_plugin.enabled = true;
    plugins.push_back(foo_plugin);
    WebPluginInfo bar_plugin;
    bar_plugin.path = FilePath(FILE_PATH_LITERAL("bar"));
    bar_plugin.name = ASCIIToUTF16("BarPlugin");
    bar_plugin.enabled = true;
    plugins.push_back(bar_plugin);

    table_model_->set_plugins(plugins);
    table_model_->ReloadSettings();
  }

  virtual void TearDown() {
    *CommandLine::ForCurrentProcess() = command_line_;
  }

 protected:
  void CheckInvariants() {
    typedef std::deque<PluginExceptionsTableModel::SettingsEntry> Entries;
    Entries& settings = table_model_->settings_;
    std::deque<int>& row_counts = table_model_->row_counts_;
    std::deque<std::string>& resources = table_model_->resources_;
    TableModel::Groups& groups = table_model_->groups_;

    EXPECT_EQ(groups.size(), row_counts.size());
    EXPECT_EQ(groups.size(), resources.size());

    int last_plugin = 0;
    int count = 0;
    for (Entries::const_iterator it = settings.begin();
         it != settings.end(); ++it) {
      // Plugin IDs are indices into |groups|.
      EXPECT_GE(it->plugin_id, 0);
      EXPECT_LT(it->plugin_id, static_cast<int>(groups.size()));
      if (it->plugin_id == last_plugin) {
        count++;
      } else {
        // Plugin IDs are ascending one by one.
        EXPECT_EQ(last_plugin+1, it->plugin_id);

        // Consecutive runs of plugins with id |x| are |row_counts[x]| long.
        EXPECT_EQ(count, row_counts[last_plugin]);
        count = 1;
        last_plugin = it->plugin_id;
      }
    }
    if (row_counts.size() > 0)
      EXPECT_EQ(count, row_counts[last_plugin]);
  }

 protected:
  MessageLoop message_loop_;
  ChromeThread ui_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<TestingPluginExceptionsTableModel> table_model_;

 private:
  CommandLine command_line_;
};

TEST_F(PluginExceptionsTableModelTest, Basic) {
  EXPECT_EQ(3, table_model_->RowCount());
  CheckInvariants();
}

TEST_F(PluginExceptionsTableModelTest, RemoveOneRow) {
  MockTableModelObserver observer;
  table_model_->SetObserver(&observer);

  EXPECT_CALL(observer, OnItemsRemoved(1, 1));
  RemoveRowsTableModel::Rows rows;
  rows.insert(1);
  table_model_->RemoveRows(rows);
  EXPECT_EQ(2, table_model_->RowCount());
  EXPECT_EQ(2, static_cast<int>(table_model_->GetGroups().size()));
  CheckInvariants();
  table_model_->SetObserver(NULL);
}

TEST_F(PluginExceptionsTableModelTest, RemoveLastRowInGroup) {
  MockTableModelObserver observer;
  table_model_->SetObserver(&observer);

  EXPECT_CALL(observer, OnModelChanged());
  RemoveRowsTableModel::Rows rows;
  rows.insert(0);
  table_model_->RemoveRows(rows);
  EXPECT_EQ(2, table_model_->RowCount());
  EXPECT_EQ(1, static_cast<int>(table_model_->GetGroups().size()));
  CheckInvariants();
  table_model_->SetObserver(NULL);
}

TEST_F(PluginExceptionsTableModelTest, RemoveAllRows) {
  MockTableModelObserver observer;
  table_model_->SetObserver(&observer);

  EXPECT_CALL(observer, OnItemsRemoved(0, 3));
  table_model_->RemoveAll();
  EXPECT_EQ(0, table_model_->RowCount());
  EXPECT_EQ(0, static_cast<int>(table_model_->GetGroups().size()));
  CheckInvariants();
  table_model_->SetObserver(NULL);
}

}  // namespace plugin_test_internal
