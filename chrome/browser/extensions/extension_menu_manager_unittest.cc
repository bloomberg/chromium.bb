// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_menu_manager.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/context_menu_params.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::_;
using testing::AtLeast;
using testing::Return;
using testing::SaveArg;

// Base class for tests.
class ExtensionMenuManagerTest : public testing::Test {
 public:
  ExtensionMenuManagerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        manager_(&profile_),
        next_id_(1) {
  }

  // Returns a test item.
  ExtensionMenuItem* CreateTestItem(Extension* extension) {
    ExtensionMenuItem::Type type = ExtensionMenuItem::NORMAL;
    ExtensionMenuItem::ContextList contexts(ExtensionMenuItem::ALL);
    ExtensionMenuItem::Id id(NULL, extension->id());
    id.uid = next_id_++;
    return new ExtensionMenuItem(id, "test", false, true, type, contexts);
  }

  // Returns a test item with the given string ID.
  ExtensionMenuItem* CreateTestItemWithID(Extension* extension,
                                          const std::string& string_id) {
    ExtensionMenuItem::Type type = ExtensionMenuItem::NORMAL;
    ExtensionMenuItem::ContextList contexts(ExtensionMenuItem::ALL);
    ExtensionMenuItem::Id id(NULL, extension->id());
    id.string_uid = string_id;
    return new ExtensionMenuItem(id, "test", false, true, type, contexts);
  }

  // Creates and returns a test Extension. The caller does *not* own the return
  // value.
  Extension* AddExtension(std::string name) {
    scoped_refptr<Extension> extension = prefs_.AddExtension(name);
    extensions_.push_back(extension);
    return extension;
  }

 protected:
  TestingProfile profile_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  ExtensionMenuManager manager_;
  ExtensionList extensions_;
  TestExtensionPrefs prefs_;
  int next_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionMenuManagerTest);
};

// Tests adding, getting, and removing items.
TEST_F(ExtensionMenuManagerTest, AddGetRemoveItems) {
  Extension* extension = AddExtension("test");

  // Add a new item, make sure you can get it back.
  ExtensionMenuItem* item1 = CreateTestItem(extension);
  ASSERT_TRUE(item1 != NULL);
  ASSERT_TRUE(manager_.AddContextItem(extension, item1));
  ASSERT_EQ(item1, manager_.GetItemById(item1->id()));
  const ExtensionMenuItem::List* items =
      manager_.MenuItems(item1->extension_id());
  ASSERT_EQ(1u, items->size());
  ASSERT_EQ(item1, items->at(0));

  // Add a second item, make sure it comes back too.
  ExtensionMenuItem* item2 = CreateTestItemWithID(extension, "id2");
  ASSERT_TRUE(manager_.AddContextItem(extension, item2));
  ASSERT_EQ(item2, manager_.GetItemById(item2->id()));
  items = manager_.MenuItems(item2->extension_id());
  ASSERT_EQ(2u, items->size());
  ASSERT_EQ(item1, items->at(0));
  ASSERT_EQ(item2, items->at(1));

  // Try adding item 3, then removing it.
  ExtensionMenuItem* item3 = CreateTestItem(extension);
  ExtensionMenuItem::Id id3 = item3->id();
  std::string extension_id = item3->extension_id();
  ASSERT_TRUE(manager_.AddContextItem(extension, item3));
  ASSERT_EQ(item3, manager_.GetItemById(id3));
  ASSERT_EQ(3u, manager_.MenuItems(extension_id)->size());
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id3));
  ASSERT_EQ(NULL, manager_.GetItemById(id3));
  ASSERT_EQ(2u, manager_.MenuItems(extension_id)->size());

  // Make sure removing a non-existent item returns false.
  ExtensionMenuItem::Id id(NULL, extension->id());
  id.uid = id3.uid + 50;
  ASSERT_FALSE(manager_.RemoveContextMenuItem(id));

  // Make sure adding an item with the same string ID returns false.
  scoped_ptr<ExtensionMenuItem> item2too(
      CreateTestItemWithID(extension, "id2"));
  ASSERT_FALSE(manager_.AddContextItem(extension, item2too.get()));

  // But the same string ID should not collide with another extension.
  Extension* extension2 = AddExtension("test2");
  ExtensionMenuItem* item2other = CreateTestItemWithID(extension2, "id2");
  ASSERT_TRUE(manager_.AddContextItem(extension2, item2other));
}

// Test adding/removing child items.
TEST_F(ExtensionMenuManagerTest, ChildFunctions) {
  Extension* extension1 = AddExtension("1111");
  Extension* extension2 = AddExtension("2222");
  Extension* extension3 = AddExtension("3333");

  ExtensionMenuItem* item1 = CreateTestItem(extension1);
  ExtensionMenuItem* item2 = CreateTestItem(extension2);
  ExtensionMenuItem* item2_child = CreateTestItemWithID(extension2, "2child");
  ExtensionMenuItem* item2_grandchild = CreateTestItem(extension2);

  // This third item we expect to fail inserting, so we use a scoped_ptr to make
  // sure it gets deleted.
  scoped_ptr<ExtensionMenuItem> item3(CreateTestItem(extension3));

  // Add in the first two items.
  ASSERT_TRUE(manager_.AddContextItem(extension1, item1));
  ASSERT_TRUE(manager_.AddContextItem(extension2, item2));

  ExtensionMenuItem::Id id1 = item1->id();
  ExtensionMenuItem::Id id2 = item2->id();

  // Try adding item3 as a child of item2 - this should fail because item3 has
  // a different extension id.
  ASSERT_FALSE(manager_.AddChildItem(id2, item3.get()));

  // Add item2_child as a child of item2.
  ExtensionMenuItem::Id id2_child = item2_child->id();
  ASSERT_TRUE(manager_.AddChildItem(id2, item2_child));
  ASSERT_EQ(1, item2->child_count());
  ASSERT_EQ(0, item1->child_count());
  ASSERT_EQ(item2_child, manager_.GetItemById(id2_child));

  ASSERT_EQ(1u, manager_.MenuItems(item1->extension_id())->size());
  ASSERT_EQ(item1, manager_.MenuItems(item1->extension_id())->at(0));

  // Add item2_grandchild as a child of item2_child, then remove it.
  ExtensionMenuItem::Id id2_grandchild = item2_grandchild->id();
  ASSERT_TRUE(manager_.AddChildItem(id2_child, item2_grandchild));
  ASSERT_EQ(1, item2->child_count());
  ASSERT_EQ(1, item2_child->child_count());
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id2_grandchild));

  // We should only get 1 thing back when asking for item2's extension id, since
  // it has a child item.
  ASSERT_EQ(1u, manager_.MenuItems(item2->extension_id())->size());
  ASSERT_EQ(item2, manager_.MenuItems(item2->extension_id())->at(0));

  // Remove child2_item.
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id2_child));
  ASSERT_EQ(1u, manager_.MenuItems(item2->extension_id())->size());
  ASSERT_EQ(item2, manager_.MenuItems(item2->extension_id())->at(0));
  ASSERT_EQ(0, item2->child_count());
}

// Tests that deleting a parent properly removes descendants.
TEST_F(ExtensionMenuManagerTest, DeleteParent) {
  Extension* extension = AddExtension("1111");

  // Set up 5 items to add.
  ExtensionMenuItem* item1 = CreateTestItem(extension);
  ExtensionMenuItem* item2 = CreateTestItem(extension);
  ExtensionMenuItem* item3 = CreateTestItemWithID(extension, "id3");
  ExtensionMenuItem* item4 = CreateTestItemWithID(extension, "id4");
  ExtensionMenuItem* item5 = CreateTestItem(extension);
  ExtensionMenuItem* item6 = CreateTestItem(extension);
  ExtensionMenuItem::Id item1_id = item1->id();
  ExtensionMenuItem::Id item2_id = item2->id();
  ExtensionMenuItem::Id item3_id = item3->id();
  ExtensionMenuItem::Id item4_id = item4->id();
  ExtensionMenuItem::Id item5_id = item5->id();
  ExtensionMenuItem::Id item6_id = item6->id();

  // Add the items in the hierarchy
  // item1 -> item2 -> item3 -> item4 -> item5 -> item6.
  ASSERT_TRUE(manager_.AddContextItem(extension, item1));
  ASSERT_TRUE(manager_.AddChildItem(item1_id, item2));
  ASSERT_TRUE(manager_.AddChildItem(item2_id, item3));
  ASSERT_TRUE(manager_.AddChildItem(item3_id, item4));
  ASSERT_TRUE(manager_.AddChildItem(item4_id, item5));
  ASSERT_TRUE(manager_.AddChildItem(item5_id, item6));
  ASSERT_EQ(item1, manager_.GetItemById(item1_id));
  ASSERT_EQ(item2, manager_.GetItemById(item2_id));
  ASSERT_EQ(item3, manager_.GetItemById(item3_id));
  ASSERT_EQ(item4, manager_.GetItemById(item4_id));
  ASSERT_EQ(item5, manager_.GetItemById(item5_id));
  ASSERT_EQ(item6, manager_.GetItemById(item6_id));
  ASSERT_EQ(1u, manager_.MenuItems(extension->id())->size());
  ASSERT_EQ(6u, manager_.items_by_id_.size());

  // Remove item6 (a leaf node).
  ASSERT_TRUE(manager_.RemoveContextMenuItem(item6_id));
  ASSERT_EQ(item1, manager_.GetItemById(item1_id));
  ASSERT_EQ(item2, manager_.GetItemById(item2_id));
  ASSERT_EQ(item3, manager_.GetItemById(item3_id));
  ASSERT_EQ(item4, manager_.GetItemById(item4_id));
  ASSERT_EQ(item5, manager_.GetItemById(item5_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item6_id));
  ASSERT_EQ(1u, manager_.MenuItems(extension->id())->size());
  ASSERT_EQ(5u, manager_.items_by_id_.size());

  // Remove item4 and make sure item5 is gone as well.
  ASSERT_TRUE(manager_.RemoveContextMenuItem(item4_id));
  ASSERT_EQ(item1, manager_.GetItemById(item1_id));
  ASSERT_EQ(item2, manager_.GetItemById(item2_id));
  ASSERT_EQ(item3, manager_.GetItemById(item3_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item4_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item5_id));
  ASSERT_EQ(1u, manager_.MenuItems(extension->id())->size());
  ASSERT_EQ(3u, manager_.items_by_id_.size());

  // Now remove item1 and make sure item2 and item3 are gone as well.
  ASSERT_TRUE(manager_.RemoveContextMenuItem(item1_id));
  ASSERT_EQ(NULL, manager_.MenuItems(extension->id()));
  ASSERT_EQ(0u, manager_.items_by_id_.size());
  ASSERT_EQ(NULL, manager_.GetItemById(item1_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item2_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item3_id));
}

// Tests changing parents.
TEST_F(ExtensionMenuManagerTest, ChangeParent) {
  Extension* extension1 = AddExtension("1111");

  // First create two items and add them both to the manager.
  ExtensionMenuItem* item1 = CreateTestItem(extension1);
  ExtensionMenuItem* item2 = CreateTestItem(extension1);

  ASSERT_TRUE(manager_.AddContextItem(extension1, item1));
  ASSERT_TRUE(manager_.AddContextItem(extension1, item2));

  const ExtensionMenuItem::List* items =
      manager_.MenuItems(item1->extension_id());
  ASSERT_EQ(2u, items->size());
  ASSERT_EQ(item1, items->at(0));
  ASSERT_EQ(item2, items->at(1));

  // Now create a third item, initially add it as a child of item1, then move
  // it to be a child of item2.
  ExtensionMenuItem* item3 = CreateTestItem(extension1);

  ASSERT_TRUE(manager_.AddChildItem(item1->id(), item3));
  ASSERT_EQ(1, item1->child_count());
  ASSERT_EQ(item3, item1->children()[0]);

  ASSERT_TRUE(manager_.ChangeParent(item3->id(), &item2->id()));
  ASSERT_EQ(0, item1->child_count());
  ASSERT_EQ(1, item2->child_count());
  ASSERT_EQ(item3, item2->children()[0]);

  // Move item2 to be a child of item1.
  ASSERT_TRUE(manager_.ChangeParent(item2->id(), &item1->id()));
  ASSERT_EQ(1, item1->child_count());
  ASSERT_EQ(item2, item1->children()[0]);
  ASSERT_EQ(1, item2->child_count());
  ASSERT_EQ(item3, item2->children()[0]);

  // Since item2 was a top-level item but is no longer, we should only have 1
  // top-level item.
  items = manager_.MenuItems(item1->extension_id());
  ASSERT_EQ(1u, items->size());
  ASSERT_EQ(item1, items->at(0));

  // Move item3 back to being a child of item1, so it's now a sibling of item2.
  ASSERT_TRUE(manager_.ChangeParent(item3->id(), &item1->id()));
  ASSERT_EQ(2, item1->child_count());
  ASSERT_EQ(item2, item1->children()[0]);
  ASSERT_EQ(item3, item1->children()[1]);

  // Try switching item3 to be the parent of item1 - this should fail.
  ASSERT_FALSE(manager_.ChangeParent(item1->id(), &item3->id()));
  ASSERT_EQ(0, item3->child_count());
  ASSERT_EQ(2, item1->child_count());
  ASSERT_EQ(item2, item1->children()[0]);
  ASSERT_EQ(item3, item1->children()[1]);
  items = manager_.MenuItems(item1->extension_id());
  ASSERT_EQ(1u, items->size());
  ASSERT_EQ(item1, items->at(0));

  // Move item2 to be a top-level item.
  ASSERT_TRUE(manager_.ChangeParent(item2->id(), NULL));
  items = manager_.MenuItems(item1->extension_id());
  ASSERT_EQ(2u, items->size());
  ASSERT_EQ(item1, items->at(0));
  ASSERT_EQ(item2, items->at(1));
  ASSERT_EQ(1, item1->child_count());
  ASSERT_EQ(item3, item1->children()[0]);

  // Make sure you can't move a node to be a child of another extension's item.
  Extension* extension2 = AddExtension("2222");
  ExtensionMenuItem* item4 = CreateTestItem(extension2);
  ASSERT_TRUE(manager_.AddContextItem(extension2, item4));
  ASSERT_FALSE(manager_.ChangeParent(item4->id(), &item1->id()));
  ASSERT_FALSE(manager_.ChangeParent(item1->id(), &item4->id()));

  // Make sure you can't make an item be it's own parent.
  ASSERT_FALSE(manager_.ChangeParent(item1->id(), &item1->id()));
}

// Tests that we properly remove an extension's menu item when that extension is
// unloaded.
TEST_F(ExtensionMenuManagerTest, ExtensionUnloadRemovesMenuItems) {
  content::NotificationService* notifier =
      content::NotificationService::current();
  ASSERT_TRUE(notifier != NULL);

  // Create a test extension.
  Extension* extension1 = AddExtension("1111");

  // Create an ExtensionMenuItem and put it into the manager.
  ExtensionMenuItem* item1 = CreateTestItem(extension1);
  ExtensionMenuItem::Id id1 = item1->id();
  ASSERT_EQ(extension1->id(), item1->extension_id());
  ASSERT_TRUE(manager_.AddContextItem(extension1, item1));
  ASSERT_EQ(1u, manager_.MenuItems(extension1->id())->size());

  // Create a menu item with a different extension id and add it to the manager.
  Extension* extension2 = AddExtension("2222");
  ExtensionMenuItem* item2 = CreateTestItem(extension2);
  ASSERT_NE(item1->extension_id(), item2->extension_id());
  ASSERT_TRUE(manager_.AddContextItem(extension2, item2));

  // Notify that the extension was unloaded, and make sure the right item is
  // gone.
  UnloadedExtensionInfo details(
      extension1, extension_misc::UNLOAD_REASON_DISABLE);
  notifier->Notify(chrome::NOTIFICATION_EXTENSION_UNLOADED,
                   content::Source<Profile>(&profile_),
                   content::Details<UnloadedExtensionInfo>(&details));
  ASSERT_EQ(NULL, manager_.MenuItems(extension1->id()));
  ASSERT_EQ(1u, manager_.MenuItems(extension2->id())->size());
  ASSERT_TRUE(manager_.GetItemById(id1) == NULL);
  ASSERT_TRUE(manager_.GetItemById(item2->id()) != NULL);
}

// A mock message service for tests of ExtensionMenuManager::ExecuteCommand.
class MockExtensionEventRouter : public ExtensionEventRouter {
 public:
  explicit MockExtensionEventRouter(Profile* profile) :
      ExtensionEventRouter(profile) {}

  MOCK_METHOD6(DispatchEventToExtension,
               void(const std::string& extension_id,
                    const std::string& event_name,
                    const std::string& event_args,
                    Profile* source_profile,
                    const GURL& event_url,
                    ExtensionEventRouter::UserGestureState state));


 private:
  DISALLOW_COPY_AND_ASSIGN(MockExtensionEventRouter);
};

// A mock profile for tests of ExtensionMenuManager::ExecuteCommand.
class MockTestingProfile : public TestingProfile {
 public:
  MockTestingProfile() {}
  MOCK_METHOD0(GetExtensionEventRouter, ExtensionEventRouter*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTestingProfile);
};

// Tests the RemoveAll functionality.
TEST_F(ExtensionMenuManagerTest, RemoveAll) {
  // Try removing all items for an extension id that doesn't have any items.
  manager_.RemoveAllContextItems("CCCC");

  // Add 2 top-level and one child item for extension 1.
  Extension* extension1 = AddExtension("1111");
  ExtensionMenuItem* item1 = CreateTestItem(extension1);
  ExtensionMenuItem* item2 = CreateTestItem(extension1);
  ExtensionMenuItem* item3 = CreateTestItem(extension1);
  ASSERT_TRUE(manager_.AddContextItem(extension1, item1));
  ASSERT_TRUE(manager_.AddContextItem(extension1, item2));
  ASSERT_TRUE(manager_.AddChildItem(item1->id(), item3));

  // Add one top-level item for extension 2.
  Extension* extension2 = AddExtension("2222");
  ExtensionMenuItem* item4 = CreateTestItem(extension2);
  ASSERT_TRUE(manager_.AddContextItem(extension2, item4));

  EXPECT_EQ(2u, manager_.MenuItems(extension1->id())->size());
  EXPECT_EQ(1u, manager_.MenuItems(extension2->id())->size());

  // Remove extension2's item.
  manager_.RemoveAllContextItems(extension2->id());
  EXPECT_EQ(2u, manager_.MenuItems(extension1->id())->size());
  EXPECT_EQ(NULL, manager_.MenuItems(extension2->id()));

  // Remove extension1's items.
  manager_.RemoveAllContextItems(extension1->id());
  EXPECT_EQ(NULL, manager_.MenuItems(extension1->id()));
}

// Tests that removing all items one-by-one doesn't leave an entry around.
TEST_F(ExtensionMenuManagerTest, RemoveOneByOne) {
  // Add 2 test items.
  Extension* extension1 = AddExtension("1111");
  ExtensionMenuItem* item1 = CreateTestItem(extension1);
  ExtensionMenuItem* item2 = CreateTestItem(extension1);
  ExtensionMenuItem* item3 = CreateTestItemWithID(extension1, "id3");
  ASSERT_TRUE(manager_.AddContextItem(extension1, item1));
  ASSERT_TRUE(manager_.AddContextItem(extension1, item2));
  ASSERT_TRUE(manager_.AddContextItem(extension1, item3));

  ASSERT_FALSE(manager_.context_items_.empty());

  manager_.RemoveContextMenuItem(item3->id());
  manager_.RemoveContextMenuItem(item1->id());
  manager_.RemoveContextMenuItem(item2->id());

  ASSERT_TRUE(manager_.context_items_.empty());
}

TEST_F(ExtensionMenuManagerTest, ExecuteCommand) {
  MockTestingProfile profile;

  scoped_ptr<MockExtensionEventRouter> mock_event_router(
      new MockExtensionEventRouter(&profile));

  content::ContextMenuParams params;
  params.media_type = WebKit::WebContextMenuData::MediaTypeImage;
  params.src_url = GURL("http://foo.bar/image.png");
  params.page_url = GURL("http://foo.bar");
  params.selection_text = ASCIIToUTF16("Hello World");
  params.is_editable = false;

  Extension* extension = AddExtension("test");
  ExtensionMenuItem* item = CreateTestItem(extension);
  ExtensionMenuItem::Id id = item->id();
  ASSERT_TRUE(manager_.AddContextItem(extension, item));

  EXPECT_CALL(profile, GetExtensionEventRouter())
      .Times(1)
      .WillOnce(Return(mock_event_router.get()));

  // Use the magic of googlemock to save a parameter to our mock's
  // DispatchEventToExtension method into event_args.
  std::string event_args;
  std::string expected_event_name = "contextMenus";
  EXPECT_CALL(*mock_event_router.get(),
              DispatchEventToExtension(
                  item->extension_id(),
                  expected_event_name,
                  _,
                  &profile,
                  GURL(),
                  ExtensionEventRouter::USER_GESTURE_ENABLED))
      .Times(1)
      .WillOnce(SaveArg<2>(&event_args));

  manager_.ExecuteCommand(&profile, NULL /* tab_contents */, params, id);

  // Parse the json event_args, which should turn into a 2-element list where
  // the first element is a dictionary we want to inspect for the correct
  // values.
  scoped_ptr<Value> result(
      base::JSONReader::Read(event_args, base::JSON_ALLOW_TRAILING_COMMAS));
  Value* value = result.get();
  ASSERT_TRUE(result.get() != NULL);
  ASSERT_EQ(Value::TYPE_LIST, value->GetType());
  ListValue* list = static_cast<ListValue*>(value);
  ASSERT_EQ(2u, list->GetSize());

  DictionaryValue* info;
  ASSERT_TRUE(list->GetDictionary(0, &info));

  int tmp_id = 0;
  ASSERT_TRUE(info->GetInteger("menuItemId", &tmp_id));
  ASSERT_EQ(id.uid, tmp_id);

  std::string tmp;
  ASSERT_TRUE(info->GetString("mediaType", &tmp));
  ASSERT_EQ("image", tmp);
  ASSERT_TRUE(info->GetString("srcUrl", &tmp));
  ASSERT_EQ(params.src_url.spec(), tmp);
  ASSERT_TRUE(info->GetString("pageUrl", &tmp));
  ASSERT_EQ(params.page_url.spec(), tmp);

  string16 tmp16;
  ASSERT_TRUE(info->GetString("selectionText", &tmp16));
  ASSERT_EQ(params.selection_text, tmp16);

  bool bool_tmp = true;
  ASSERT_TRUE(info->GetBoolean("editable", &bool_tmp));
  ASSERT_EQ(params.is_editable, bool_tmp);
}

// Test that there is always only one radio item selected.
TEST_F(ExtensionMenuManagerTest, SanitizeRadioButtons) {
  Extension* extension = AddExtension("test");

  // A single unchecked item should get checked
  ExtensionMenuItem* item1 = CreateTestItem(extension);

  item1->set_type(ExtensionMenuItem::RADIO);
  item1->SetChecked(false);
  ASSERT_FALSE(item1->checked());
  manager_.AddContextItem(extension, item1);
  ASSERT_TRUE(item1->checked());

  // In a run of two unchecked items, the first should get selected.
  item1->SetChecked(false);
  ExtensionMenuItem* item2 = CreateTestItem(extension);
  item2->set_type(ExtensionMenuItem::RADIO);
  item2->SetChecked(false);
  ASSERT_FALSE(item1->checked());
  ASSERT_FALSE(item2->checked());
  manager_.AddContextItem(extension, item2);
  ASSERT_TRUE(item1->checked());
  ASSERT_FALSE(item2->checked());

  // If multiple items are checked, only the last item should get checked.
  item1->SetChecked(true);
  item2->SetChecked(true);
  ASSERT_TRUE(item1->checked());
  ASSERT_TRUE(item2->checked());
  manager_.ItemUpdated(item1->id());
  ASSERT_FALSE(item1->checked());
  ASSERT_TRUE(item2->checked());

  // If the checked item is removed, the new first item should get checked.
  item1->SetChecked(false);
  item2->SetChecked(true);
  ASSERT_FALSE(item1->checked());
  ASSERT_TRUE(item2->checked());
  manager_.RemoveContextMenuItem(item2->id());
  item2 = NULL;
  ASSERT_TRUE(item1->checked());

  // If a checked item is added to a run that already has a checked item,
  // then the new item should get checked.
  item1->SetChecked(true);
  ExtensionMenuItem* new_item = CreateTestItem(extension);
  new_item->set_type(ExtensionMenuItem::RADIO);
  new_item->SetChecked(true);
  ASSERT_TRUE(item1->checked());
  ASSERT_TRUE(new_item->checked());
  manager_.AddContextItem(extension, new_item);
  ASSERT_FALSE(item1->checked());
  ASSERT_TRUE(new_item->checked());
  // Make sure that children are checked as well.
  ExtensionMenuItem* parent = CreateTestItem(extension);
  manager_.AddContextItem(extension, parent);
  ExtensionMenuItem* child1 = CreateTestItem(extension);
  child1->set_type(ExtensionMenuItem::RADIO);
  child1->SetChecked(false);
  ExtensionMenuItem* child2 = CreateTestItem(extension);
  child2->set_type(ExtensionMenuItem::RADIO);
  child2->SetChecked(true);
  ASSERT_FALSE(child1->checked());
  ASSERT_TRUE(child2->checked());

  manager_.AddChildItem(parent->id(), child1);
  ASSERT_TRUE(child1->checked());

  manager_.AddChildItem(parent->id(), child2);
  ASSERT_FALSE(child1->checked());
  ASSERT_TRUE(child2->checked());

  // Removing the checked item from the children should cause the
  // remaining child to be checked.
  manager_.RemoveContextMenuItem(child2->id());
  child2 = NULL;
  ASSERT_TRUE(child1->checked());

  // This should NOT cause |new_item| to be deseleted because
  // |parent| will be seperating the two runs of radio items.
  manager_.ChangeParent(child1->id(), NULL);
  ASSERT_TRUE(new_item->checked());
  ASSERT_TRUE(child1->checked());

  // Removing |parent| should cause only |child1| to be selected.
  manager_.RemoveContextMenuItem(parent->id());
  parent = NULL;
  ASSERT_FALSE(new_item->checked());
  ASSERT_TRUE(child1->checked());
}
