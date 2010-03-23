// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_menu_manager.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/glue/context_menu.h"

using testing::_;
using testing::AtLeast;
using testing::Return;
using testing::SaveArg;

// Base class for tests.
class ExtensionMenuManagerTest : public testing::Test {
 public:
  ExtensionMenuManagerTest() {}
  ~ExtensionMenuManagerTest() {}

  // Returns a test item with some default values you can override if you want
  // to by passing in |properties| (currently just extension_id). Caller owns
  // the return value and is responsible for freeing it.
  static ExtensionMenuItem* CreateTestItem(DictionaryValue* properties) {
    std::string extension_id = "0123456789";  // A default dummy value.
    if (properties && properties->HasKey(L"extension_id"))
      EXPECT_TRUE(properties->GetString(L"extension_id", &extension_id));

    ExtensionMenuItem::Type type = ExtensionMenuItem::NORMAL;
    ExtensionMenuItem::ContextList contexts(ExtensionMenuItem::ALL);
    ExtensionMenuItem::ContextList enabled_contexts = contexts;
    std::string title = "test";

    return new ExtensionMenuItem(extension_id, title, false, type, contexts,
                                 enabled_contexts);
  }

 protected:
  ExtensionMenuManager manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionMenuManagerTest);
};

// Tests adding, getting, and removing items.
TEST_F(ExtensionMenuManagerTest, AddGetRemoveItems) {
  // Add a new item, make sure you can get it back.
  ExtensionMenuItem* item1 = CreateTestItem(NULL);
  ASSERT_TRUE(item1 != NULL);
  int id1 = manager_.AddContextItem(item1);  // Ownership transferred.
  ASSERT_GT(id1, 0);
  ASSERT_EQ(item1, manager_.GetItemById(id1));
  std::vector<const ExtensionMenuItem*> items =
      manager_.MenuItems(item1->extension_id());
  ASSERT_EQ(1u, items.size());
  ASSERT_EQ(item1, items[0]);

  // Add a second item, make sure it comes back too.
  ExtensionMenuItem* item2 = CreateTestItem(NULL);
  int id2 = manager_.AddContextItem(item2);  // Ownership transferred.
  ASSERT_GT(id2, 0);
  ASSERT_NE(id1, id2);
  ASSERT_EQ(item2, manager_.GetItemById(id2));
  items = manager_.MenuItems(item2->extension_id());
  ASSERT_EQ(2u, items.size());
  ASSERT_EQ(item1, items[0]);
  ASSERT_EQ(item2, items[1]);

  // Try adding item 3, then removing it.
  ExtensionMenuItem* item3 = CreateTestItem(NULL);
  std::string extension_id = item3->extension_id();
  int id3 = manager_.AddContextItem(item3);  // Ownership transferred.
  ASSERT_GT(id3, 0);
  ASSERT_EQ(item3, manager_.GetItemById(id3));
  ASSERT_EQ(3u, manager_.MenuItems(extension_id).size());
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id3));
  ASSERT_EQ(NULL, manager_.GetItemById(id3));
  ASSERT_EQ(2u, manager_.MenuItems(extension_id).size());

  // Make sure removing a non-existent item returns false.
  ASSERT_FALSE(manager_.RemoveContextMenuItem(5));
}

// Test adding/removing child items.
TEST_F(ExtensionMenuManagerTest, ChildFunctions) {
  DictionaryValue properties;
  properties.SetString(L"extension_id", "1111");
  ExtensionMenuItem* item1 = CreateTestItem(&properties);

  properties.SetString(L"extension_id", "2222");
  ExtensionMenuItem* item2 = CreateTestItem(&properties);
  ExtensionMenuItem* item2_child = CreateTestItem(&properties);
  ExtensionMenuItem* item2_grandchild = CreateTestItem(&properties);

  // This third item we expect to fail inserting, so we use a scoped_ptr to make
  // sure it gets deleted.
  properties.SetString(L"extension_id", "3333");
  scoped_ptr<ExtensionMenuItem> item3(CreateTestItem(&properties));

  // Add in the first two items.
  int id1 = manager_.AddContextItem(item1);  // Ownership transferred.
  int id2 = manager_.AddContextItem(item2);  // Ownership transferred.

  ASSERT_NE(id1, id2);

  // Try adding item3 as a child of item2 - this should fail because item3 has
  // a different extension id.
  ASSERT_EQ(0, manager_.AddChildItem(id2, item3.get()));

  // Add item2_child as a child of item2.
  int id2_child = manager_.AddChildItem(id2, item2_child);
  ASSERT_GT(id2_child, 0);
  ASSERT_EQ(1, item2->child_count());
  ASSERT_EQ(0, item1->child_count());
  ASSERT_EQ(item2_child, manager_.GetItemById(id2_child));

  ASSERT_EQ(1u, manager_.MenuItems(item1->extension_id()).size());
  ASSERT_EQ(item1, manager_.MenuItems(item1->extension_id()).at(0));

  // Add item2_grandchild as a child of item2_child, then remove it.
  int id2_grandchild = manager_.AddChildItem(id2_child, item2_grandchild);
  ASSERT_GT(id2_grandchild, 0);
  ASSERT_EQ(1, item2->child_count());
  ASSERT_EQ(1, item2_child->child_count());
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id2_grandchild));

  // We should only get 1 thing back when asking for item2's extension id, since
  // it has a child item.
  ASSERT_EQ(1u, manager_.MenuItems(item2->extension_id()).size());
  ASSERT_EQ(item2, manager_.MenuItems(item2->extension_id()).at(0));

  // Remove child2_item.
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id2_child));
  ASSERT_EQ(1u, manager_.MenuItems(item2->extension_id()).size());
  ASSERT_EQ(item2, manager_.MenuItems(item2->extension_id()).at(0));
  ASSERT_EQ(0, item2->child_count());
}

// Tests that we properly remove an extension's menu item when that extension is
// unloaded.
TEST_F(ExtensionMenuManagerTest, ExtensionUnloadRemovesMenuItems) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  NotificationService* notifier = NotificationService::current();
  ASSERT_TRUE(notifier != NULL);

  // Create a test extension.
  DictionaryValue extension_properties;
  extension_properties.SetString(extension_manifest_keys::kVersion, "1");
  extension_properties.SetString(extension_manifest_keys::kName, "Test");
  Extension extension(temp_dir.path().AppendASCII("extension"));
  std::string errors;
  ASSERT_TRUE(extension.InitFromValue(extension_properties,
                                      false,  // No public key required.
                                      &errors)) << errors;

  // Create an ExtensionMenuItem and put it into the manager.
  DictionaryValue item_properties;
  item_properties.SetString(L"extension_id", extension.id());
  ExtensionMenuItem* item1 = CreateTestItem(&item_properties);
  ASSERT_EQ(extension.id(), item1->extension_id());
  int id1 = manager_.AddContextItem(item1);  // Ownership transferred.
  ASSERT_GT(id1, 0);
  ASSERT_EQ(1u, manager_.MenuItems(extension.id()).size());

  // Create a menu item with a different extension id and add it to the manager.
  std::string alternate_extension_id = "0000";
  item_properties.SetString(L"extension_id", alternate_extension_id);
  ExtensionMenuItem* item2 = CreateTestItem(&item_properties);
  ASSERT_NE(item1->extension_id(), item2->extension_id());
  int id2 = manager_.AddContextItem(item2);  // Ownership transferred.
  ASSERT_GT(id2, 0);

  // Notify that the extension was unloaded, and make sure the right item is
  // gone.
  notifier->Notify(NotificationType::EXTENSION_UNLOADED,
                   Source<Profile>(NULL),
                   Details<Extension>(&extension));
  ASSERT_EQ(0u, manager_.MenuItems(extension.id()).size());
  ASSERT_EQ(1u, manager_.MenuItems(alternate_extension_id).size());
  ASSERT_TRUE(manager_.GetItemById(id1) == NULL);
  ASSERT_TRUE(manager_.GetItemById(id2) != NULL);
}

// A mock message service for tests of ExtensionMenuManager::ExecuteCommand.
class MockExtensionMessageService : public ExtensionMessageService {
 public:
  explicit MockExtensionMessageService(Profile* profile) :
      ExtensionMessageService(profile) {}

  MOCK_METHOD3(DispatchEventToRenderers, void(const std::string& event_name,
                                              const std::string& event_args,
                                              bool has_incognito_data));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockExtensionMessageService);
};

// A mock profile for tests of ExtensionMenuManager::ExecuteCommand.
class MockTestingProfile : public TestingProfile {
 public:
  MockTestingProfile() {}
  MOCK_METHOD0(GetExtensionMessageService, ExtensionMessageService*());
  MOCK_METHOD0(IsOffTheRecord, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTestingProfile);
};

TEST_F(ExtensionMenuManagerTest, ExecuteCommand) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  MockTestingProfile profile;

  scoped_refptr<MockExtensionMessageService> mock_message_service =
      new MockExtensionMessageService(&profile);

  ContextMenuParams params;
  params.media_type = WebKit::WebContextMenuData::MediaTypeImage;
  params.src_url = GURL("http://foo.bar/image.png");
  params.page_url = GURL("http://foo.bar");
  params.selection_text = L"Hello World";
  params.is_editable = false;

  ExtensionMenuItem* item = CreateTestItem(NULL);
  int id = manager_.AddContextItem(item);  // Ownership transferred.
  ASSERT_GT(id, 0);

  EXPECT_CALL(profile, GetExtensionMessageService())
      .Times(1)
      .WillOnce(Return(mock_message_service.get()));

  EXPECT_CALL(profile, IsOffTheRecord())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(false));

  // Use the magic of googlemock to save a parameter to our mock's
  // DispatchEventToRenderers method into event_args.
  std::string event_args;
  std::string expected_event_name = "contextMenu/" + item->extension_id();
  EXPECT_CALL(*mock_message_service.get(),
              DispatchEventToRenderers(expected_event_name, _,
                                       profile.IsOffTheRecord()))
      .Times(1)
      .WillOnce(SaveArg<1>(&event_args));

  manager_.ExecuteCommand(&profile, NULL /* tab_contents */, params, id);

  // Parse the json event_args, which should turn into a 2-element list where
  // the first element is a dictionary we want to inspect for the correct
  // values.
  scoped_ptr<Value> result(base::JSONReader::Read(event_args, true));
  Value* value = result.get();
  ASSERT_TRUE(result.get() != NULL);
  ASSERT_EQ(Value::TYPE_LIST, value->GetType());
  ListValue* list = static_cast<ListValue*>(value);
  ASSERT_EQ(2u, list->GetSize());

  DictionaryValue* info;
  ASSERT_TRUE(list->GetDictionary(0, &info));

  int tmp_id = 0;
  ASSERT_TRUE(info->GetInteger(L"menuItemId", &tmp_id));
  ASSERT_EQ(id, tmp_id);

  std::string tmp;
  ASSERT_TRUE(info->GetString(L"mediaType", &tmp));
  ASSERT_EQ("IMAGE", tmp);
  ASSERT_TRUE(info->GetString(L"srcUrl", &tmp));
  ASSERT_EQ(params.src_url.spec(), tmp);
  ASSERT_TRUE(info->GetString(L"mainFrameUrl", &tmp));
  ASSERT_EQ(params.page_url.spec(), tmp);

  std::wstring wide_tmp;
  ASSERT_TRUE(info->GetString(L"selectionText", &wide_tmp));
  ASSERT_EQ(params.selection_text, wide_tmp);

  bool bool_tmp = true;
  ASSERT_TRUE(info->GetBoolean(L"editable", &bool_tmp));
  ASSERT_EQ(params.is_editable, bool_tmp);
}
