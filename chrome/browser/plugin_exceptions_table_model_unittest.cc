// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/mock_plugin_exceptions_table_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/table_model_observer.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/webplugininfo.h"

// Can't be an internal namespace because PluginExceptionsTableModel declares
// as a friend.
namespace plugin_test_internal {

using ::testing::_;
using ::testing::Invoke;

class MockTableModelObserver : public ui::TableModelObserver {
 public:
  explicit MockTableModelObserver(ui::TableModel* model)
      : model_(model) {
    ON_CALL(*this, OnItemsRemoved(_, _))
        .WillByDefault(
            Invoke(this, &MockTableModelObserver::CheckOnItemsRemoved));
  }

  MOCK_METHOD0(OnModelChanged, void());
  MOCK_METHOD2(OnItemsChanged, void(int start, int length));
  MOCK_METHOD2(OnItemsAdded, void(int start, int length));
  MOCK_METHOD2(OnItemsRemoved, void(int start, int length));

 private:
  void CheckOnItemsRemoved(int start, int length) {
    if (!model_)
      return;
    // This method is called *after* the items have been removed, so we check if
    // the first removed item was still inside the correct range.
    EXPECT_LT(start, model_->RowCount() + 1);
  }

  ui::TableModel* model_;
};

}  // namespace plugin_test_internal

using ::testing::InSequence;

class PluginExceptionsTableModelTest : public testing::Test {
 public:
  PluginExceptionsTableModelTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        command_line_(CommandLine::ForCurrentProcess(),
                      *CommandLine::ForCurrentProcess()) {}

  virtual void SetUp() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableResourceContentSettings);

    profile_.reset(new TestingProfile());

    HostContentSettingsMap* map = profile_->GetHostContentSettingsMap();

    ContentSettingsPattern example_com("[*.]example.com");
    ContentSettingsPattern moose_org("[*.]moose.org");
    map->SetContentSetting(example_com,
                           CONTENT_SETTINGS_TYPE_PLUGINS,
                           "a-foo",
                           CONTENT_SETTING_ALLOW);
    map->SetContentSetting(moose_org,
                           CONTENT_SETTINGS_TYPE_PLUGINS,
                           "b-bar",
                           CONTENT_SETTING_BLOCK);
    map->SetContentSetting(example_com,
                           CONTENT_SETTINGS_TYPE_PLUGINS,
                           "b-bar",
                           CONTENT_SETTING_ALLOW);

    table_model_.reset(new MockPluginExceptionsTableModel(map, NULL));

    std::vector<webkit::npapi::PluginGroup> plugins;
    webkit::npapi::WebPluginInfo foo_plugin;
    foo_plugin.path = FilePath(FILE_PATH_LITERAL("a-foo"));
    foo_plugin.name = ASCIIToUTF16("FooPlugin");
    foo_plugin.enabled =
        webkit::npapi::WebPluginInfo::USER_ENABLED_POLICY_UNMANAGED;
    scoped_ptr<webkit::npapi::PluginGroup> foo_group(
        webkit::npapi::PluginGroup::FromWebPluginInfo(foo_plugin));
    plugins.push_back(*foo_group);

    webkit::npapi::WebPluginInfo bar_plugin;
    bar_plugin.path = FilePath(FILE_PATH_LITERAL("b-bar"));
    bar_plugin.name = ASCIIToUTF16("BarPlugin");
    bar_plugin.enabled =
        webkit::npapi::WebPluginInfo::USER_ENABLED_POLICY_UNMANAGED;
    scoped_ptr<webkit::npapi::PluginGroup> bar_group(
        webkit::npapi::PluginGroup::FromWebPluginInfo(bar_plugin));
    plugins.push_back(*bar_group);

    table_model_->set_plugins(plugins);
    table_model_->ReloadSettings();
  }

 protected:
  void CheckInvariants() const {
    typedef std::vector<PluginExceptionsTableModel::SettingsEntry> Entries;
    const Entries& settings = table_model_->settings_;
    const std::vector<int>& row_counts = table_model_->row_counts_;
    const std::vector<std::string>& resources = table_model_->resources_;
    const ui::TableModel::Groups& groups = table_model_->groups_;

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
    if (!row_counts.empty())
      EXPECT_EQ(count, row_counts[last_plugin]);
  }

 protected:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<MockPluginExceptionsTableModel> table_model_;

 private:
  AutoReset<CommandLine> command_line_;
};

TEST_F(PluginExceptionsTableModelTest, Basic) {
  EXPECT_EQ(3, table_model_->RowCount());
  CheckInvariants();
}

TEST_F(PluginExceptionsTableModelTest, RemoveOneRow) {
  plugin_test_internal::MockTableModelObserver observer(table_model_.get());
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
  plugin_test_internal::MockTableModelObserver observer(table_model_.get());
  table_model_->SetObserver(&observer);

  EXPECT_CALL(observer, OnModelChanged());
  RemoveRowsTableModel::Rows rows;
  rows.insert(0);
  table_model_->RemoveRows(rows);
  EXPECT_EQ(2, table_model_->RowCount());
  EXPECT_EQ(1, static_cast<int>(table_model_->GetGroups().size()));
  CheckInvariants();

  HostContentSettingsMap* map = profile_->GetHostContentSettingsMap();
  EXPECT_CALL(observer, OnModelChanged());
  map->SetContentSetting(ContentSettingsPattern("[*.]blurp.net"),
                         CONTENT_SETTINGS_TYPE_PLUGINS,
                         "b-bar",
                         CONTENT_SETTING_BLOCK);
  EXPECT_EQ(3, table_model_->RowCount());

  InSequence s;
  EXPECT_CALL(observer, OnItemsRemoved(2, 1));
  EXPECT_CALL(observer, OnItemsRemoved(0, 1));
  rows.clear();
  rows.insert(0);
  rows.insert(2);
  table_model_->RemoveRows(rows);
  EXPECT_EQ(1, table_model_->RowCount());
  EXPECT_EQ(1, static_cast<int>(table_model_->GetGroups().size()));
  CheckInvariants();

  table_model_->SetObserver(NULL);
}

TEST_F(PluginExceptionsTableModelTest, RemoveAllRows) {
  plugin_test_internal::MockTableModelObserver observer(table_model_.get());
  table_model_->SetObserver(&observer);

  EXPECT_CALL(observer, OnModelChanged());
  table_model_->RemoveAll();
  EXPECT_EQ(0, table_model_->RowCount());
  EXPECT_EQ(0, static_cast<int>(table_model_->GetGroups().size()));
  CheckInvariants();
  table_model_->SetObserver(NULL);
}
