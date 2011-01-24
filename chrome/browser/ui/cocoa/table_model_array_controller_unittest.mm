// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/table_model_array_controller.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/mock_plugin_exceptions_table_model.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugininfo.h"

class TableModelArrayControllerTest : public CocoaTest {
 public:
  TableModelArrayControllerTest()
      : command_line_(CommandLine::ForCurrentProcess(),
                      *CommandLine::ForCurrentProcess()) {}

  virtual void SetUp() {
    CocoaTest::SetUp();

    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableResourceContentSettings);

    TestingProfile* profile = browser_helper_.profile();
    HostContentSettingsMap* map = profile->GetHostContentSettingsMap();

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

    model_.reset(new MockPluginExceptionsTableModel(map, NULL));

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
    webkit::npapi::WebPluginInfo blurp_plugin;
    blurp_plugin.path = FilePath(FILE_PATH_LITERAL("c-blurp"));
    blurp_plugin.name = ASCIIToUTF16("BlurpPlugin");
    blurp_plugin.enabled =
        webkit::npapi::WebPluginInfo::USER_ENABLED_POLICY_UNMANAGED;
    scoped_ptr<webkit::npapi::PluginGroup> blurp_group(
        webkit::npapi::PluginGroup::FromWebPluginInfo(blurp_plugin));
    plugins.push_back(*blurp_group);

    model_->set_plugins(plugins);
    model_->LoadSettings();

    id content = [NSMutableArray array];
    controller_.reset(
        [[TableModelArrayController alloc] initWithContent:content]);
    NSDictionary* columns = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithInt:IDS_EXCEPTIONS_HOSTNAME_HEADER], @"title",
        [NSNumber numberWithInt:IDS_EXCEPTIONS_ACTION_HEADER], @"action",
        nil];
    [controller_.get() bindToTableModel:model_.get()
                          withColumns:columns
                     groupTitleColumn:@"title"];
  }

 protected:
  BrowserTestHelper browser_helper_;
  scoped_ptr<MockPluginExceptionsTableModel> model_;
  scoped_nsobject<TableModelArrayController> controller_;

 private:
  AutoReset<CommandLine> command_line_;
};

TEST_F(TableModelArrayControllerTest, CheckTitles) {
  NSArray* titles = [[controller_.get() arrangedObjects] valueForKey:@"title"];
  EXPECT_NSEQ(@"(\n"
              @"    FooPlugin,\n"
              @"    \"[*.]example.com\",\n"
              @"    BarPlugin,\n"
              @"    \"[*.]example.com\",\n"
              @"    \"[*.]moose.org\"\n"
              @")",
              [titles description]);
}

TEST_F(TableModelArrayControllerTest, RemoveRows) {
  NSArrayController* controller = controller_.get();
  [controller setSelectionIndex:1];
  [controller remove:nil];
  NSArray* titles = [[controller arrangedObjects] valueForKey:@"title"];
  EXPECT_NSEQ(@"(\n"
              @"    BarPlugin,\n"
              @"    \"[*.]example.com\",\n"
              @"    \"[*.]moose.org\"\n"
              @")",
              [titles description]);

  [controller setSelectionIndex:2];
  [controller remove:nil];
  titles = [[controller arrangedObjects] valueForKey:@"title"];
  EXPECT_NSEQ(@"(\n"
              @"    BarPlugin,\n"
              @"    \"[*.]example.com\"\n"
              @")",
              [titles description]);
}

TEST_F(TableModelArrayControllerTest, RemoveAll) {
  [controller_.get() removeAll:nil];
  EXPECT_EQ(0u, [[controller_.get() arrangedObjects] count]);
}

TEST_F(TableModelArrayControllerTest, AddException) {
  TestingProfile* profile = browser_helper_.profile();
  HostContentSettingsMap* map = profile->GetHostContentSettingsMap();
  ContentSettingsPattern example_com("[*.]example.com");
  map->SetContentSetting(example_com,
                         CONTENT_SETTINGS_TYPE_PLUGINS,
                         "c-blurp",
                         CONTENT_SETTING_BLOCK);

  NSArrayController* controller = controller_.get();
  NSArray* titles = [[controller arrangedObjects] valueForKey:@"title"];
  EXPECT_NSEQ(@"(\n"
              @"    FooPlugin,\n"
              @"    \"[*.]example.com\",\n"
              @"    BarPlugin,\n"
              @"    \"[*.]example.com\",\n"
              @"    \"[*.]moose.org\",\n"
              @"    BlurpPlugin,\n"
              @"    \"[*.]example.com\"\n"
              @")",
              [titles description]);
  NSMutableIndexSet* indexes = [NSMutableIndexSet indexSetWithIndex:1];
  [indexes addIndex:6];
  [controller setSelectionIndexes:indexes];
  [controller remove:nil];
  titles = [[controller arrangedObjects] valueForKey:@"title"];
  EXPECT_NSEQ(@"(\n"
              @"    BarPlugin,\n"
              @"    \"[*.]example.com\",\n"
              @"    \"[*.]moose.org\"\n"
              @")",
              [titles description]);
}

