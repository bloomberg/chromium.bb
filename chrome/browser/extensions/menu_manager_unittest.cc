// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::_;
using testing::AtLeast;
using testing::DeleteArg;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;

namespace extensions {

namespace context_menus = api::context_menus;

// Base class for tests.
class MenuManagerTest : public testing::Test {
 public:
  MenuManagerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        profile_(new TestingProfile()),
        manager_(profile_.get(),
                 ExtensionSystem::Get(profile_.get())->state_store()),
        prefs_(message_loop_.message_loop_proxy().get()),
        next_id_(1) {}

  virtual void TearDown() OVERRIDE {
    prefs_.pref_service()->CommitPendingWrite();
    message_loop_.RunUntilIdle();
  }

  // Returns a test item.
  MenuItem* CreateTestItem(Extension* extension, bool incognito = false) {
    MenuItem::Type type = MenuItem::NORMAL;
    MenuItem::ContextList contexts(MenuItem::ALL);
    const MenuItem::ExtensionKey key(extension->id());
    MenuItem::Id id(incognito, key);
    id.uid = next_id_++;
    return new MenuItem(id, "test", false, true, type, contexts);
  }

  // Returns a test item with the given string ID.
  MenuItem* CreateTestItemWithID(Extension* extension,
                                 const std::string& string_id) {
    MenuItem::Type type = MenuItem::NORMAL;
    MenuItem::ContextList contexts(MenuItem::ALL);
    const MenuItem::ExtensionKey key(extension->id());
    MenuItem::Id id(false, key);
    id.string_uid = string_id;
    return new MenuItem(id, "test", false, true, type, contexts);
  }

  // Creates and returns a test Extension. The caller does *not* own the return
  // value.
  Extension* AddExtension(const std::string& name) {
    scoped_refptr<Extension> extension = prefs_.AddExtension(name);
    extensions_.push_back(extension);
    return extension.get();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_ptr<TestingProfile> profile_;

  MenuManager manager_;
  ExtensionList extensions_;
  TestExtensionPrefs prefs_;
  int next_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuManagerTest);
};

// Tests adding, getting, and removing items.
TEST_F(MenuManagerTest, AddGetRemoveItems) {
  Extension* extension = AddExtension("test");

  // Add a new item, make sure you can get it back.
  MenuItem* item1 = CreateTestItem(extension);
  ASSERT_TRUE(item1 != NULL);
  ASSERT_TRUE(manager_.AddContextItem(extension, item1));
  ASSERT_EQ(item1, manager_.GetItemById(item1->id()));
  const MenuItem::List* items = manager_.MenuItems(item1->id().extension_key);
  ASSERT_EQ(1u, items->size());
  ASSERT_EQ(item1, items->at(0));

  // Add a second item, make sure it comes back too.
  MenuItem* item2 = CreateTestItemWithID(extension, "id2");
  ASSERT_TRUE(manager_.AddContextItem(extension, item2));
  ASSERT_EQ(item2, manager_.GetItemById(item2->id()));
  items = manager_.MenuItems(item2->id().extension_key);
  ASSERT_EQ(2u, items->size());
  ASSERT_EQ(item1, items->at(0));
  ASSERT_EQ(item2, items->at(1));

  // Try adding item 3, then removing it.
  MenuItem* item3 = CreateTestItem(extension);
  MenuItem::Id id3 = item3->id();
  const MenuItem::ExtensionKey extension_key3(item3->id().extension_key);
  ASSERT_TRUE(manager_.AddContextItem(extension, item3));
  ASSERT_EQ(item3, manager_.GetItemById(id3));
  ASSERT_EQ(3u, manager_.MenuItems(extension_key3)->size());
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id3));
  ASSERT_EQ(NULL, manager_.GetItemById(id3));
  ASSERT_EQ(2u, manager_.MenuItems(extension_key3)->size());

  // Make sure removing a non-existent item returns false.
  const MenuItem::ExtensionKey key(extension->id());
  MenuItem::Id id(false, key);
  id.uid = id3.uid + 50;
  ASSERT_FALSE(manager_.RemoveContextMenuItem(id));

  // Make sure adding an item with the same string ID returns false.
  scoped_ptr<MenuItem> item2too(CreateTestItemWithID(extension, "id2"));
  ASSERT_FALSE(manager_.AddContextItem(extension, item2too.get()));

  // But the same string ID should not collide with another extension.
  Extension* extension2 = AddExtension("test2");
  MenuItem* item2other = CreateTestItemWithID(extension2, "id2");
  ASSERT_TRUE(manager_.AddContextItem(extension2, item2other));
}

// Test adding/removing child items.
TEST_F(MenuManagerTest, ChildFunctions) {
  Extension* extension1 = AddExtension("1111");
  Extension* extension2 = AddExtension("2222");
  Extension* extension3 = AddExtension("3333");

  MenuItem* item1 = CreateTestItem(extension1);
  MenuItem* item2 = CreateTestItem(extension2);
  MenuItem* item2_child = CreateTestItemWithID(extension2, "2child");
  MenuItem* item2_grandchild = CreateTestItem(extension2);

  // This third item we expect to fail inserting, so we use a scoped_ptr to make
  // sure it gets deleted.
  scoped_ptr<MenuItem> item3(CreateTestItem(extension3));

  // Add in the first two items.
  ASSERT_TRUE(manager_.AddContextItem(extension1, item1));
  ASSERT_TRUE(manager_.AddContextItem(extension2, item2));

  MenuItem::Id id1 = item1->id();
  MenuItem::Id id2 = item2->id();

  // Try adding item3 as a child of item2 - this should fail because item3 has
  // a different extension id.
  ASSERT_FALSE(manager_.AddChildItem(id2, item3.get()));

  // Add item2_child as a child of item2.
  MenuItem::Id id2_child = item2_child->id();
  ASSERT_TRUE(manager_.AddChildItem(id2, item2_child));
  ASSERT_EQ(1, item2->child_count());
  ASSERT_EQ(0, item1->child_count());
  ASSERT_EQ(item2_child, manager_.GetItemById(id2_child));

  ASSERT_EQ(1u, manager_.MenuItems(item1->id().extension_key)->size());
  ASSERT_EQ(item1, manager_.MenuItems(item1->id().extension_key)->at(0));

  // Add item2_grandchild as a child of item2_child, then remove it.
  MenuItem::Id id2_grandchild = item2_grandchild->id();
  ASSERT_TRUE(manager_.AddChildItem(id2_child, item2_grandchild));
  ASSERT_EQ(1, item2->child_count());
  ASSERT_EQ(1, item2_child->child_count());
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id2_grandchild));

  // We should only get 1 thing back when asking for item2's extension id, since
  // it has a child item.
  ASSERT_EQ(1u, manager_.MenuItems(item2->id().extension_key)->size());
  ASSERT_EQ(item2, manager_.MenuItems(item2->id().extension_key)->at(0));

  // Remove child2_item.
  ASSERT_TRUE(manager_.RemoveContextMenuItem(id2_child));
  ASSERT_EQ(1u, manager_.MenuItems(item2->id().extension_key)->size());
  ASSERT_EQ(item2, manager_.MenuItems(item2->id().extension_key)->at(0));
  ASSERT_EQ(0, item2->child_count());
}

TEST_F(MenuManagerTest, PopulateFromValue) {
  Extension* extension = AddExtension("test");

  bool incognito = true;
  int type = MenuItem::CHECKBOX;
  std::string title("TITLE");
  bool checked = true;
  bool enabled = true;
  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::SELECTION);
  int contexts_value = 0;
  ASSERT_TRUE(contexts.ToValue()->GetAsInteger(&contexts_value));

  base::ListValue* document_url_patterns(new base::ListValue());
  document_url_patterns->Append(
      new base::StringValue("http://www.google.com/*"));
  document_url_patterns->Append(
      new base::StringValue("http://www.reddit.com/*"));

  base::ListValue* target_url_patterns(new base::ListValue());
  target_url_patterns->Append(
      new base::StringValue("http://www.yahoo.com/*"));
  target_url_patterns->Append(
      new base::StringValue("http://www.facebook.com/*"));

  base::DictionaryValue value;
  value.SetBoolean("incognito", incognito);
  value.SetString("string_uid", std::string());
  value.SetInteger("type", type);
  value.SetString("title", title);
  value.SetBoolean("checked", checked);
  value.SetBoolean("enabled", enabled);
  value.SetInteger("contexts", contexts_value);
  value.Set("document_url_patterns", document_url_patterns);
  value.Set("target_url_patterns", target_url_patterns);

  std::string error;
  scoped_ptr<MenuItem> item(MenuItem::Populate(extension->id(), value, &error));
  ASSERT_TRUE(item.get());

  EXPECT_EQ(extension->id(), item->extension_id());
  EXPECT_EQ(incognito, item->incognito());
  EXPECT_EQ(title, item->title());
  EXPECT_EQ(checked, item->checked());
  EXPECT_EQ(item->checked(), item->checked());
  EXPECT_EQ(enabled, item->enabled());
  EXPECT_EQ(contexts, item->contexts());

  URLPatternSet document_url_pattern_set;
  document_url_pattern_set.Populate(*document_url_patterns,
                                    URLPattern::SCHEME_ALL,
                                    true,
                                    &error);
  EXPECT_EQ(document_url_pattern_set, item->document_url_patterns());

  URLPatternSet target_url_pattern_set;
  target_url_pattern_set.Populate(*target_url_patterns,
                                   URLPattern::SCHEME_ALL,
                                   true,
                                   &error);
  EXPECT_EQ(target_url_pattern_set, item->target_url_patterns());
}

// Tests that deleting a parent properly removes descendants.
TEST_F(MenuManagerTest, DeleteParent) {
  Extension* extension = AddExtension("1111");

  // Set up 5 items to add.
  MenuItem* item1 = CreateTestItem(extension);
  MenuItem* item2 = CreateTestItem(extension);
  MenuItem* item3 = CreateTestItemWithID(extension, "id3");
  MenuItem* item4 = CreateTestItemWithID(extension, "id4");
  MenuItem* item5 = CreateTestItem(extension);
  MenuItem* item6 = CreateTestItem(extension);
  MenuItem::Id item1_id = item1->id();
  MenuItem::Id item2_id = item2->id();
  MenuItem::Id item3_id = item3->id();
  MenuItem::Id item4_id = item4->id();
  MenuItem::Id item5_id = item5->id();
  MenuItem::Id item6_id = item6->id();
  const MenuItem::ExtensionKey key(extension->id());

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
  ASSERT_EQ(1u, manager_.MenuItems(key)->size());
  ASSERT_EQ(6u, manager_.items_by_id_.size());

  // Remove item6 (a leaf node).
  ASSERT_TRUE(manager_.RemoveContextMenuItem(item6_id));
  ASSERT_EQ(item1, manager_.GetItemById(item1_id));
  ASSERT_EQ(item2, manager_.GetItemById(item2_id));
  ASSERT_EQ(item3, manager_.GetItemById(item3_id));
  ASSERT_EQ(item4, manager_.GetItemById(item4_id));
  ASSERT_EQ(item5, manager_.GetItemById(item5_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item6_id));
  ASSERT_EQ(1u, manager_.MenuItems(key)->size());
  ASSERT_EQ(5u, manager_.items_by_id_.size());

  // Remove item4 and make sure item5 is gone as well.
  ASSERT_TRUE(manager_.RemoveContextMenuItem(item4_id));
  ASSERT_EQ(item1, manager_.GetItemById(item1_id));
  ASSERT_EQ(item2, manager_.GetItemById(item2_id));
  ASSERT_EQ(item3, manager_.GetItemById(item3_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item4_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item5_id));
  ASSERT_EQ(1u, manager_.MenuItems(key)->size());
  ASSERT_EQ(3u, manager_.items_by_id_.size());

  // Now remove item1 and make sure item2 and item3 are gone as well.
  ASSERT_TRUE(manager_.RemoveContextMenuItem(item1_id));
  ASSERT_EQ(NULL, manager_.MenuItems(key));
  ASSERT_EQ(0u, manager_.items_by_id_.size());
  ASSERT_EQ(NULL, manager_.GetItemById(item1_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item2_id));
  ASSERT_EQ(NULL, manager_.GetItemById(item3_id));
}

// Tests changing parents.
TEST_F(MenuManagerTest, ChangeParent) {
  Extension* extension1 = AddExtension("1111");

  // First create two items and add them both to the manager.
  MenuItem* item1 = CreateTestItem(extension1);
  MenuItem* item2 = CreateTestItem(extension1);

  ASSERT_TRUE(manager_.AddContextItem(extension1, item1));
  ASSERT_TRUE(manager_.AddContextItem(extension1, item2));

  const MenuItem::List* items = manager_.MenuItems(item1->id().extension_key);
  ASSERT_EQ(2u, items->size());
  ASSERT_EQ(item1, items->at(0));
  ASSERT_EQ(item2, items->at(1));

  // Now create a third item, initially add it as a child of item1, then move
  // it to be a child of item2.
  MenuItem* item3 = CreateTestItem(extension1);

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
  items = manager_.MenuItems(item1->id().extension_key);
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
  items = manager_.MenuItems(item1->id().extension_key);
  ASSERT_EQ(1u, items->size());
  ASSERT_EQ(item1, items->at(0));

  // Move item2 to be a top-level item.
  ASSERT_TRUE(manager_.ChangeParent(item2->id(), NULL));
  items = manager_.MenuItems(item1->id().extension_key);
  ASSERT_EQ(2u, items->size());
  ASSERT_EQ(item1, items->at(0));
  ASSERT_EQ(item2, items->at(1));
  ASSERT_EQ(1, item1->child_count());
  ASSERT_EQ(item3, item1->children()[0]);

  // Make sure you can't move a node to be a child of another extension's item.
  Extension* extension2 = AddExtension("2222");
  MenuItem* item4 = CreateTestItem(extension2);
  ASSERT_TRUE(manager_.AddContextItem(extension2, item4));
  ASSERT_FALSE(manager_.ChangeParent(item4->id(), &item1->id()));
  ASSERT_FALSE(manager_.ChangeParent(item1->id(), &item4->id()));

  // Make sure you can't make an item be it's own parent.
  ASSERT_FALSE(manager_.ChangeParent(item1->id(), &item1->id()));
}

// Tests that we properly remove an extension's menu item when that extension is
// unloaded.
TEST_F(MenuManagerTest, ExtensionUnloadRemovesMenuItems) {
  content::NotificationService* notifier =
      content::NotificationService::current();
  ASSERT_TRUE(notifier != NULL);

  // Create a test extension.
  Extension* extension1 = AddExtension("1111");

  // Create an MenuItem and put it into the manager.
  MenuItem* item1 = CreateTestItem(extension1);
  MenuItem::Id id1 = item1->id();
  ASSERT_EQ(extension1->id(), item1->extension_id());
  ASSERT_TRUE(manager_.AddContextItem(extension1, item1));
  ASSERT_EQ(
      1u, manager_.MenuItems(MenuItem::ExtensionKey(extension1->id()))->size());

  // Create a menu item with a different extension id and add it to the manager.
  Extension* extension2 = AddExtension("2222");
  MenuItem* item2 = CreateTestItem(extension2);
  ASSERT_NE(item1->extension_id(), item2->extension_id());
  ASSERT_TRUE(manager_.AddContextItem(extension2, item2));

  // Notify that the extension was unloaded, and make sure the right item is
  // gone.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_.get());
  registry->TriggerOnUnloaded(extension1,
                              UnloadedExtensionInfo::REASON_DISABLE);

  ASSERT_EQ(NULL, manager_.MenuItems(MenuItem::ExtensionKey(extension1->id())));
  ASSERT_EQ(
      1u, manager_.MenuItems(MenuItem::ExtensionKey(extension2->id()))->size());
  ASSERT_TRUE(manager_.GetItemById(id1) == NULL);
  ASSERT_TRUE(manager_.GetItemById(item2->id()) != NULL);
}

// A mock message service for tests of MenuManager::ExecuteCommand.
class MockEventRouter : public EventRouter {
 public:
  explicit MockEventRouter(Profile* profile) : EventRouter(profile, NULL) {}

  MOCK_METHOD6(DispatchEventToExtensionMock,
               void(const std::string& extension_id,
                    const std::string& event_name,
                    base::ListValue* event_args,
                    content::BrowserContext* source_context,
                    const GURL& event_url,
                    EventRouter::UserGestureState state));

  virtual void DispatchEventToExtension(const std::string& extension_id,
                                        scoped_ptr<Event> event) {
    DispatchEventToExtensionMock(extension_id,
                                 event->event_name,
                                 event->event_args.release(),
                                 event->restrict_to_browser_context,
                                 event->event_url,
                                 event->user_gesture);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventRouter);
};

// A mock ExtensionSystem to serve our MockEventRouter.
class MockExtensionSystem : public TestExtensionSystem {
 public:
  explicit MockExtensionSystem(Profile* profile)
      : TestExtensionSystem(profile) {}

  virtual EventRouter* event_router() OVERRIDE {
    if (!mock_event_router_)
      mock_event_router_.reset(new MockEventRouter(profile_));
    return mock_event_router_.get();
  }

 private:
  scoped_ptr<MockEventRouter> mock_event_router_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionSystem);
};

KeyedService* BuildMockExtensionSystem(content::BrowserContext* profile) {
  return new MockExtensionSystem(static_cast<Profile*>(profile));
}

// Tests the RemoveAll functionality.
TEST_F(MenuManagerTest, RemoveAll) {
  // Try removing all items for an extension id that doesn't have any items.
  manager_.RemoveAllContextItems(MenuItem::ExtensionKey("CCCC"));

  // Add 2 top-level and one child item for extension 1.
  Extension* extension1 = AddExtension("1111");
  MenuItem* item1 = CreateTestItem(extension1);
  MenuItem* item2 = CreateTestItem(extension1);
  MenuItem* item3 = CreateTestItem(extension1);
  ASSERT_TRUE(manager_.AddContextItem(extension1, item1));
  ASSERT_TRUE(manager_.AddContextItem(extension1, item2));
  ASSERT_TRUE(manager_.AddChildItem(item1->id(), item3));

  // Add one top-level item for extension 2.
  Extension* extension2 = AddExtension("2222");
  MenuItem* item4 = CreateTestItem(extension2);
  ASSERT_TRUE(manager_.AddContextItem(extension2, item4));

  const MenuItem::ExtensionKey key1(extension1->id());
  const MenuItem::ExtensionKey key2(extension2->id());
  EXPECT_EQ(2u, manager_.MenuItems(key1)->size());
  EXPECT_EQ(1u, manager_.MenuItems(key2)->size());

  // Remove extension2's item.
  manager_.RemoveAllContextItems(key2);
  EXPECT_EQ(2u, manager_.MenuItems(key1)->size());
  EXPECT_EQ(NULL, manager_.MenuItems(key2));

  // Remove extension1's items.
  manager_.RemoveAllContextItems(key1);
  EXPECT_EQ(NULL, manager_.MenuItems(key1));
}

// Tests that removing all items one-by-one doesn't leave an entry around.
TEST_F(MenuManagerTest, RemoveOneByOne) {
  // Add 2 test items.
  Extension* extension1 = AddExtension("1111");
  MenuItem* item1 = CreateTestItem(extension1);
  MenuItem* item2 = CreateTestItem(extension1);
  MenuItem* item3 = CreateTestItemWithID(extension1, "id3");
  ASSERT_TRUE(manager_.AddContextItem(extension1, item1));
  ASSERT_TRUE(manager_.AddContextItem(extension1, item2));
  ASSERT_TRUE(manager_.AddContextItem(extension1, item3));

  ASSERT_FALSE(manager_.context_items_.empty());

  manager_.RemoveContextMenuItem(item3->id());
  manager_.RemoveContextMenuItem(item1->id());
  manager_.RemoveContextMenuItem(item2->id());

  ASSERT_TRUE(manager_.context_items_.empty());
}

TEST_F(MenuManagerTest, ExecuteCommand) {
  TestingProfile profile;

  MockExtensionSystem* mock_extension_system =
      static_cast<MockExtensionSystem*>(ExtensionSystemFactory::GetInstance()->
          SetTestingFactoryAndUse(&profile, &BuildMockExtensionSystem));
  MockEventRouter* mock_event_router =
      static_cast<MockEventRouter*>(mock_extension_system->event_router());

  content::ContextMenuParams params;
  params.media_type = blink::WebContextMenuData::MediaTypeImage;
  params.src_url = GURL("http://foo.bar/image.png");
  params.page_url = GURL("http://foo.bar");
  params.selection_text = base::ASCIIToUTF16("Hello World");
  params.is_editable = false;

  Extension* extension = AddExtension("test");
  MenuItem* parent = CreateTestItem(extension);
  MenuItem* item = CreateTestItem(extension);
  MenuItem::Id parent_id = parent->id();
  MenuItem::Id id = item->id();
  ASSERT_TRUE(manager_.AddContextItem(extension, parent));
  ASSERT_TRUE(manager_.AddChildItem(parent->id(), item));

  // Use the magic of googlemock to save a parameter to our mock's
  // DispatchEventToExtension method into event_args.
  base::ListValue* list = NULL;
  {
    InSequence s;
    EXPECT_CALL(*mock_event_router,
                DispatchEventToExtensionMock(item->extension_id(),
                                             MenuManager::kOnContextMenus,
                                             _,
                                             &profile,
                                             GURL(),
                                             EventRouter::USER_GESTURE_ENABLED))
        .Times(1)
        .WillOnce(SaveArg<2>(&list));
    EXPECT_CALL(*mock_event_router,
              DispatchEventToExtensionMock(
                  item->extension_id(),
                  context_menus::OnClicked::kEventName,
                  _,
                  &profile,
                  GURL(),
                  EventRouter::USER_GESTURE_ENABLED))
      .Times(1)
      .WillOnce(DeleteArg<2>());
  }
  manager_.ExecuteCommand(&profile, NULL /* web_contents */, params, id);

  ASSERT_EQ(2u, list->GetSize());

  base::DictionaryValue* info;
  ASSERT_TRUE(list->GetDictionary(0, &info));

  int tmp_id = 0;
  ASSERT_TRUE(info->GetInteger("menuItemId", &tmp_id));
  ASSERT_EQ(id.uid, tmp_id);
  ASSERT_TRUE(info->GetInteger("parentMenuItemId", &tmp_id));
  ASSERT_EQ(parent_id.uid, tmp_id);

  std::string tmp;
  ASSERT_TRUE(info->GetString("mediaType", &tmp));
  ASSERT_EQ("image", tmp);
  ASSERT_TRUE(info->GetString("srcUrl", &tmp));
  ASSERT_EQ(params.src_url.spec(), tmp);
  ASSERT_TRUE(info->GetString("pageUrl", &tmp));
  ASSERT_EQ(params.page_url.spec(), tmp);

  base::string16 tmp16;
  ASSERT_TRUE(info->GetString("selectionText", &tmp16));
  ASSERT_EQ(params.selection_text, tmp16);

  bool bool_tmp = true;
  ASSERT_TRUE(info->GetBoolean("editable", &bool_tmp));
  ASSERT_EQ(params.is_editable, bool_tmp);

  delete list;
}

// Test that there is always only one radio item selected.
TEST_F(MenuManagerTest, SanitizeRadioButtons) {
  Extension* extension = AddExtension("test");

  // A single unchecked item should get checked
  MenuItem* item1 = CreateTestItem(extension);

  item1->set_type(MenuItem::RADIO);
  item1->SetChecked(false);
  ASSERT_FALSE(item1->checked());
  manager_.AddContextItem(extension, item1);
  ASSERT_TRUE(item1->checked());

  // In a run of two unchecked items, the first should get selected.
  item1->SetChecked(false);
  MenuItem* item2 = CreateTestItem(extension);
  item2->set_type(MenuItem::RADIO);
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
  MenuItem* new_item = CreateTestItem(extension);
  new_item->set_type(MenuItem::RADIO);
  new_item->SetChecked(true);
  ASSERT_TRUE(item1->checked());
  ASSERT_TRUE(new_item->checked());
  manager_.AddContextItem(extension, new_item);
  ASSERT_FALSE(item1->checked());
  ASSERT_TRUE(new_item->checked());
  // Make sure that children are checked as well.
  MenuItem* parent = CreateTestItem(extension);
  manager_.AddContextItem(extension, parent);
  MenuItem* child1 = CreateTestItem(extension);
  child1->set_type(MenuItem::RADIO);
  child1->SetChecked(false);
  MenuItem* child2 = CreateTestItem(extension);
  child2->set_type(MenuItem::RADIO);
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

// Tests the RemoveAllIncognitoContextItems functionality.
TEST_F(MenuManagerTest, RemoveAllIncognito) {
  Extension* extension1 = AddExtension("1111");
  // Add 2 top-level and one child item for extension 1
  // with incognito 'true'.
  MenuItem* item1 = CreateTestItem(extension1, true);
  MenuItem* item2 = CreateTestItem(extension1, true);
  MenuItem* item3 = CreateTestItem(extension1, true);
  ASSERT_TRUE(manager_.AddContextItem(extension1, item1));
  ASSERT_TRUE(manager_.AddContextItem(extension1, item2));
  ASSERT_TRUE(manager_.AddChildItem(item1->id(), item3));

  // Add 2 top-level and one child item for extension 1
  // with incognito 'false'.
  MenuItem* item4 = CreateTestItem(extension1);
  MenuItem* item5 = CreateTestItem(extension1);
  MenuItem* item6 = CreateTestItem(extension1);
  ASSERT_TRUE(manager_.AddContextItem(extension1, item4));
  ASSERT_TRUE(manager_.AddContextItem(extension1, item5));
  ASSERT_TRUE(manager_.AddChildItem(item4->id(), item6));

  // Add one top-level item for extension 2.
  Extension* extension2 = AddExtension("2222");
  MenuItem* item7 = CreateTestItem(extension2);
  ASSERT_TRUE(manager_.AddContextItem(extension2, item7));

  const MenuItem::ExtensionKey key1(extension1->id());
  const MenuItem::ExtensionKey key2(extension2->id());
  EXPECT_EQ(4u, manager_.MenuItems(key1)->size());
  EXPECT_EQ(1u, manager_.MenuItems(key2)->size());

  // Remove all context menu items with incognito true.
  manager_.RemoveAllIncognitoContextItems();
  EXPECT_EQ(2u, manager_.MenuItems(key1)->size());
  EXPECT_EQ(1u, manager_.MenuItems(key2)->size());
}

}  // namespace extensions
