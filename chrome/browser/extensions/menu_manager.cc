// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/menu_manager.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/guest_view/web_view/web_view_guest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/common/extensions/api/web_view_internal.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/text_elider.h"

using content::WebContents;
using extensions::ExtensionSystem;

namespace extensions {

namespace context_menus = api::context_menus;
namespace web_view = api::web_view_internal;

namespace {

// Keys for serialization to and from Value to store in the preferences.
const char kContextMenusKey[] = "context_menus";

const char kCheckedKey[] = "checked";
const char kContextsKey[] = "contexts";
const char kDocumentURLPatternsKey[] = "document_url_patterns";
const char kEnabledKey[] = "enabled";
const char kIncognitoKey[] = "incognito";
const char kParentUIDKey[] = "parent_uid";
const char kStringUIDKey[] = "string_uid";
const char kTargetURLPatternsKey[] = "target_url_patterns";
const char kTitleKey[] = "title";
const char kTypeKey[] = "type";

void SetIdKeyValue(base::DictionaryValue* properties,
                   const char* key,
                   const MenuItem::Id& id) {
  if (id.uid == 0)
    properties->SetString(key, id.string_uid);
  else
    properties->SetInteger(key, id.uid);
}

MenuItem::List MenuItemsFromValue(const std::string& extension_id,
                                  base::Value* value) {
  MenuItem::List items;

  base::ListValue* list = NULL;
  if (!value || !value->GetAsList(&list))
    return items;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* dict = NULL;
    if (!list->GetDictionary(i, &dict))
      continue;
    MenuItem* item = MenuItem::Populate(
        extension_id, *dict, NULL);
    if (!item)
      continue;
    items.push_back(item);
  }
  return items;
}

scoped_ptr<base::Value> MenuItemsToValue(const MenuItem::List& items) {
  scoped_ptr<base::ListValue> list(new base::ListValue());
  for (size_t i = 0; i < items.size(); ++i)
    list->Append(items[i]->ToValue().release());
  return scoped_ptr<base::Value>(list.release());
}

bool GetStringList(const base::DictionaryValue& dict,
                   const std::string& key,
                   std::vector<std::string>* out) {
  if (!dict.HasKey(key))
    return true;

  const base::ListValue* list = NULL;
  if (!dict.GetListWithoutPathExpansion(key, &list))
    return false;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string pattern;
    if (!list->GetString(i, &pattern))
      return false;
    out->push_back(pattern);
  }

  return true;
}

}  // namespace

MenuItem::MenuItem(const Id& id,
                   const std::string& title,
                   bool checked,
                   bool enabled,
                   Type type,
                   const ContextList& contexts)
    : id_(id),
      title_(title),
      type_(type),
      checked_(checked),
      enabled_(enabled),
      contexts_(contexts) {}

MenuItem::~MenuItem() {
  STLDeleteElements(&children_);
}

MenuItem* MenuItem::ReleaseChild(const Id& child_id,
                                 bool recursive) {
  for (List::iterator i = children_.begin(); i != children_.end(); ++i) {
    MenuItem* child = NULL;
    if ((*i)->id() == child_id) {
      child = *i;
      children_.erase(i);
      return child;
    } else if (recursive) {
      child = (*i)->ReleaseChild(child_id, recursive);
      if (child)
        return child;
    }
  }
  return NULL;
}

void MenuItem::GetFlattenedSubtree(MenuItem::List* list) {
  list->push_back(this);
  for (List::iterator i = children_.begin(); i != children_.end(); ++i)
    (*i)->GetFlattenedSubtree(list);
}

std::set<MenuItem::Id> MenuItem::RemoveAllDescendants() {
  std::set<Id> result;
  for (List::iterator i = children_.begin(); i != children_.end(); ++i) {
    MenuItem* child = *i;
    result.insert(child->id());
    std::set<Id> removed = child->RemoveAllDescendants();
    result.insert(removed.begin(), removed.end());
  }
  STLDeleteElements(&children_);
  return result;
}

base::string16 MenuItem::TitleWithReplacement(const base::string16& selection,
                                              size_t max_length) const {
  base::string16 result = base::UTF8ToUTF16(title_);
  // TODO(asargent) - Change this to properly handle %% escaping so you can
  // put "%s" in titles that won't get substituted.
  ReplaceSubstringsAfterOffset(&result, 0, base::ASCIIToUTF16("%s"), selection);

  if (result.length() > max_length)
    result = gfx::TruncateString(result, max_length);
  return result;
}

bool MenuItem::SetChecked(bool checked) {
  if (type_ != CHECKBOX && type_ != RADIO)
    return false;
  checked_ = checked;
  return true;
}

void MenuItem::AddChild(MenuItem* item) {
  item->parent_id_.reset(new Id(id_));
  children_.push_back(item);
}

scoped_ptr<base::DictionaryValue> MenuItem::ToValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  // Should only be called for extensions with event pages, which only have
  // string IDs for items.
  DCHECK_EQ(0, id_.uid);
  value->SetString(kStringUIDKey, id_.string_uid);
  value->SetBoolean(kIncognitoKey, id_.incognito);
  value->SetInteger(kTypeKey, type_);
  if (type_ != SEPARATOR)
    value->SetString(kTitleKey, title_);
  if (type_ == CHECKBOX || type_ == RADIO)
    value->SetBoolean(kCheckedKey, checked_);
  value->SetBoolean(kEnabledKey, enabled_);
  value->Set(kContextsKey, contexts_.ToValue().release());
  if (parent_id_) {
    DCHECK_EQ(0, parent_id_->uid);
    value->SetString(kParentUIDKey, parent_id_->string_uid);
  }
  value->Set(kDocumentURLPatternsKey,
             document_url_patterns_.ToValue().release());
  value->Set(kTargetURLPatternsKey, target_url_patterns_.ToValue().release());
  return value.Pass();
}

// static
MenuItem* MenuItem::Populate(const std::string& extension_id,
                             const base::DictionaryValue& value,
                             std::string* error) {
  bool incognito = false;
  if (!value.GetBoolean(kIncognitoKey, &incognito))
    return NULL;
  Id id(incognito, MenuItem::ExtensionKey(extension_id));
  if (!value.GetString(kStringUIDKey, &id.string_uid))
    return NULL;
  int type_int;
  Type type = NORMAL;
  if (!value.GetInteger(kTypeKey, &type_int))
    return NULL;
  type = static_cast<Type>(type_int);
  std::string title;
  if (type != SEPARATOR && !value.GetString(kTitleKey, &title))
    return NULL;
  bool checked = false;
  if ((type == CHECKBOX || type == RADIO) &&
      !value.GetBoolean(kCheckedKey, &checked)) {
    return NULL;
  }
  bool enabled = true;
  if (!value.GetBoolean(kEnabledKey, &enabled))
    return NULL;
  ContextList contexts;
  const base::Value* contexts_value = NULL;
  if (!value.Get(kContextsKey, &contexts_value))
    return NULL;
  if (!contexts.Populate(*contexts_value))
    return NULL;

  scoped_ptr<MenuItem> result(new MenuItem(
      id, title, checked, enabled, type, contexts));

  std::vector<std::string> document_url_patterns;
  if (!GetStringList(value, kDocumentURLPatternsKey, &document_url_patterns))
    return NULL;
  std::vector<std::string> target_url_patterns;
  if (!GetStringList(value, kTargetURLPatternsKey, &target_url_patterns))
    return NULL;

  if (!result->PopulateURLPatterns(&document_url_patterns,
                                   &target_url_patterns,
                                   error)) {
    return NULL;
  }

  // parent_id is filled in from the value, but it might not be valid. It's left
  // to be validated upon being added (via AddChildItem) to the menu manager.
  scoped_ptr<Id> parent_id(
      new Id(incognito, MenuItem::ExtensionKey(extension_id)));
  if (value.HasKey(kParentUIDKey)) {
    if (!value.GetString(kParentUIDKey, &parent_id->string_uid))
      return NULL;
    result->parent_id_.swap(parent_id);
  }
  return result.release();
}

bool MenuItem::PopulateURLPatterns(
    std::vector<std::string>* document_url_patterns,
    std::vector<std::string>* target_url_patterns,
    std::string* error) {
  if (document_url_patterns) {
    if (!document_url_patterns_.Populate(
            *document_url_patterns, URLPattern::SCHEME_ALL, true, error)) {
      return false;
    }
  }
  if (target_url_patterns) {
    if (!target_url_patterns_.Populate(
            *target_url_patterns, URLPattern::SCHEME_ALL, true, error)) {
      return false;
    }
  }
  return true;
}

// static
const char MenuManager::kOnContextMenus[] = "contextMenus";
const char MenuManager::kOnWebviewContextMenus[] =
    "webViewInternal.contextMenus";

MenuManager::MenuManager(Profile* profile, StateStore* store)
    : extension_registry_observer_(this), profile_(profile), store_(store) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllSources());
  if (store_)
    store_->RegisterKey(kContextMenusKey);
}

MenuManager::~MenuManager() {
  MenuItemMap::iterator i;
  for (i = context_items_.begin(); i != context_items_.end(); ++i) {
    STLDeleteElements(&(i->second));
  }
}

// static
MenuManager* MenuManager::Get(Profile* profile) {
  return MenuManagerFactory::GetForProfile(profile);
}

std::set<MenuItem::ExtensionKey> MenuManager::ExtensionIds() {
  std::set<MenuItem::ExtensionKey> id_set;
  for (MenuItemMap::const_iterator i = context_items_.begin();
       i != context_items_.end(); ++i) {
    id_set.insert(i->first);
  }
  return id_set;
}

const MenuItem::List* MenuManager::MenuItems(
    const MenuItem::ExtensionKey& key) {
  MenuItemMap::iterator i = context_items_.find(key);
  if (i != context_items_.end()) {
    return &(i->second);
  }
  return NULL;
}

bool MenuManager::AddContextItem(const Extension* extension, MenuItem* item) {
  const MenuItem::ExtensionKey& key = item->id().extension_key;
  // The item must have a non-empty extension id, and not have already been
  // added.
  if (key.empty() || ContainsKey(items_by_id_, item->id()))
    return false;

  DCHECK_EQ(extension->id(), key.extension_id);

  bool first_item = !ContainsKey(context_items_, key);
  context_items_[key].push_back(item);
  items_by_id_[item->id()] = item;

  if (item->type() == MenuItem::RADIO) {
    if (item->checked())
      RadioItemSelected(item);
    else
      SanitizeRadioList(context_items_[key]);
  }

  // If this is the first item for this extension, start loading its icon.
  if (first_item)
    icon_manager_.LoadIcon(profile_, extension);

  return true;
}

bool MenuManager::AddChildItem(const MenuItem::Id& parent_id,
                               MenuItem* child) {
  MenuItem* parent = GetItemById(parent_id);
  if (!parent || parent->type() != MenuItem::NORMAL ||
      parent->incognito() != child->incognito() ||
      parent->extension_id() != child->extension_id() ||
      ContainsKey(items_by_id_, child->id()))
    return false;
  parent->AddChild(child);
  items_by_id_[child->id()] = child;

  if (child->type() == MenuItem::RADIO)
    SanitizeRadioList(parent->children());
  return true;
}

bool MenuManager::DescendantOf(MenuItem* item,
                               const MenuItem::Id& ancestor_id) {
  // Work our way up the tree until we find the ancestor or NULL.
  MenuItem::Id* id = item->parent_id();
  while (id != NULL) {
    DCHECK(*id != item->id());  // Catch circular graphs.
    if (*id == ancestor_id)
      return true;
    MenuItem* next = GetItemById(*id);
    if (!next) {
      NOTREACHED();
      return false;
    }
    id = next->parent_id();
  }
  return false;
}

bool MenuManager::ChangeParent(const MenuItem::Id& child_id,
                               const MenuItem::Id* parent_id) {
  MenuItem* child = GetItemById(child_id);
  MenuItem* new_parent = parent_id ? GetItemById(*parent_id) : NULL;
  if ((parent_id && (child_id == *parent_id)) || !child ||
      (!new_parent && parent_id != NULL) ||
      (new_parent && (DescendantOf(new_parent, child_id) ||
                      child->incognito() != new_parent->incognito() ||
                      child->extension_id() != new_parent->extension_id())))
    return false;

  MenuItem::Id* old_parent_id = child->parent_id();
  if (old_parent_id != NULL) {
    MenuItem* old_parent = GetItemById(*old_parent_id);
    if (!old_parent) {
      NOTREACHED();
      return false;
    }
    MenuItem* taken =
      old_parent->ReleaseChild(child_id, false /* non-recursive search*/);
    DCHECK(taken == child);
    SanitizeRadioList(old_parent->children());
  } else {
    // This is a top-level item, so we need to pull it out of our list of
    // top-level items.
    const MenuItem::ExtensionKey& child_key = child->id().extension_key;
    MenuItemMap::iterator i = context_items_.find(child_key);
    if (i == context_items_.end()) {
      NOTREACHED();
      return false;
    }
    MenuItem::List& list = i->second;
    MenuItem::List::iterator j = std::find(list.begin(), list.end(), child);
    if (j == list.end()) {
      NOTREACHED();
      return false;
    }
    list.erase(j);
    SanitizeRadioList(list);
  }

  if (new_parent) {
    new_parent->AddChild(child);
    SanitizeRadioList(new_parent->children());
  } else {
    const MenuItem::ExtensionKey& child_key = child->id().extension_key;
    context_items_[child_key].push_back(child);
    child->parent_id_.reset(NULL);
    SanitizeRadioList(context_items_[child_key]);
  }
  return true;
}

bool MenuManager::RemoveContextMenuItem(const MenuItem::Id& id) {
  if (!ContainsKey(items_by_id_, id))
    return false;

  MenuItem* menu_item = GetItemById(id);
  DCHECK(menu_item);
  const MenuItem::ExtensionKey extension_key = id.extension_key;
  MenuItemMap::iterator i = context_items_.find(extension_key);
  if (i == context_items_.end()) {
    NOTREACHED();
    return false;
  }

  bool result = false;
  std::set<MenuItem::Id> items_removed;
  MenuItem::List& list = i->second;
  MenuItem::List::iterator j;
  for (j = list.begin(); j < list.end(); ++j) {
    // See if the current top-level item is a match.
    if ((*j)->id() == id) {
      items_removed = (*j)->RemoveAllDescendants();
      items_removed.insert(id);
      delete *j;
      list.erase(j);
      result = true;
      SanitizeRadioList(list);
      break;
    } else {
      // See if the item to remove was found as a descendant of the current
      // top-level item.
      MenuItem* child = (*j)->ReleaseChild(id, true /* recursive */);
      if (child) {
        items_removed = child->RemoveAllDescendants();
        items_removed.insert(id);
        SanitizeRadioList(GetItemById(*child->parent_id())->children());
        delete child;
        result = true;
        break;
      }
    }
  }
  DCHECK(result);  // The check at the very top should have prevented this.

  // Clear entries from the items_by_id_ map.
  std::set<MenuItem::Id>::iterator removed_iter;
  for (removed_iter = items_removed.begin();
       removed_iter != items_removed.end();
       ++removed_iter) {
    items_by_id_.erase(*removed_iter);
  }

  if (list.empty()) {
    context_items_.erase(extension_key);
    icon_manager_.RemoveIcon(extension_key.extension_id);
  }
  return result;
}

void MenuManager::RemoveAllContextItems(
    const MenuItem::ExtensionKey& extension_key) {
  MenuItem::List::iterator i;
  for (i = context_items_[extension_key].begin();
       i != context_items_[extension_key].end();
       ++i) {
    MenuItem* item = *i;
    items_by_id_.erase(item->id());

    // Remove descendants from this item and erase them from the lookup cache.
    std::set<MenuItem::Id> removed_ids = item->RemoveAllDescendants();
    std::set<MenuItem::Id>::const_iterator j;
    for (j = removed_ids.begin(); j != removed_ids.end(); ++j) {
      items_by_id_.erase(*j);
    }
  }
  STLDeleteElements(&context_items_[extension_key]);
  context_items_.erase(extension_key);
  icon_manager_.RemoveIcon(extension_key.extension_id);
}

MenuItem* MenuManager::GetItemById(const MenuItem::Id& id) const {
  std::map<MenuItem::Id, MenuItem*>::const_iterator i =
      items_by_id_.find(id);
  if (i != items_by_id_.end())
    return i->second;
  else
    return NULL;
}

void MenuManager::RadioItemSelected(MenuItem* item) {
  // If this is a child item, we need to get a handle to the list from its
  // parent. Otherwise get a handle to the top-level list.
  const MenuItem::List* list = NULL;
  if (item->parent_id()) {
    MenuItem* parent = GetItemById(*item->parent_id());
    if (!parent) {
      NOTREACHED();
      return;
    }
    list = &(parent->children());
  } else {
    const MenuItem::ExtensionKey& key = item->id().extension_key;
    if (context_items_.find(key) == context_items_.end()) {
      NOTREACHED();
      return;
    }
    list = &context_items_[key];
  }

  // Find where |item| is in the list.
  MenuItem::List::const_iterator item_location;
  for (item_location = list->begin(); item_location != list->end();
       ++item_location) {
    if (*item_location == item)
      break;
  }
  if (item_location == list->end()) {
    NOTREACHED();  // We should have found the item.
    return;
  }

  // Iterate backwards from |item| and uncheck any adjacent radio items.
  MenuItem::List::const_iterator i;
  if (item_location != list->begin()) {
    i = item_location;
    do {
      --i;
      if ((*i)->type() != MenuItem::RADIO)
        break;
      (*i)->SetChecked(false);
    } while (i != list->begin());
  }

  // Now iterate forwards from |item| and uncheck any adjacent radio items.
  for (i = item_location + 1; i != list->end(); ++i) {
    if ((*i)->type() != MenuItem::RADIO)
      break;
    (*i)->SetChecked(false);
  }
}

static void AddURLProperty(base::DictionaryValue* dictionary,
                           const std::string& key, const GURL& url) {
  if (!url.is_empty())
    dictionary->SetString(key, url.possibly_invalid_spec());
}

void MenuManager::ExecuteCommand(Profile* profile,
                                 WebContents* web_contents,
                                 const content::ContextMenuParams& params,
                                 const MenuItem::Id& menu_item_id) {
  EventRouter* event_router = EventRouter::Get(profile);
  if (!event_router)
    return;

  MenuItem* item = GetItemById(menu_item_id);
  if (!item)
    return;

  // ExtensionService/Extension can be NULL in unit tests :(
  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();
  const Extension* extension =
      service ? service->extensions()->GetByID(item->extension_id()) : NULL;

  if (item->type() == MenuItem::RADIO)
    RadioItemSelected(item);

  scoped_ptr<base::ListValue> args(new base::ListValue());

  base::DictionaryValue* properties = new base::DictionaryValue();
  SetIdKeyValue(properties, "menuItemId", item->id());
  if (item->parent_id())
    SetIdKeyValue(properties, "parentMenuItemId", *item->parent_id());

  switch (params.media_type) {
    case blink::WebContextMenuData::MediaTypeImage:
      properties->SetString("mediaType", "image");
      break;
    case blink::WebContextMenuData::MediaTypeVideo:
      properties->SetString("mediaType", "video");
      break;
    case blink::WebContextMenuData::MediaTypeAudio:
      properties->SetString("mediaType", "audio");
      break;
    default:  {}  // Do nothing.
  }

  AddURLProperty(properties, "linkUrl", params.unfiltered_link_url);
  AddURLProperty(properties, "srcUrl", params.src_url);
  AddURLProperty(properties, "pageUrl", params.page_url);
  AddURLProperty(properties, "frameUrl", params.frame_url);

  if (params.selection_text.length() > 0)
    properties->SetString("selectionText", params.selection_text);

  properties->SetBoolean("editable", params.is_editable);

  WebViewGuest* webview_guest = WebViewGuest::FromWebContents(web_contents);
  if (webview_guest) {
    // This is used in web_view_internalcustom_bindings.js.
    // The property is not exposed to developer API.
    properties->SetInteger("webviewInstanceId",
                           webview_guest->view_instance_id());
  }

  args->Append(properties);

  // Add the tab info to the argument list.
  // No tab info in a platform app.
  if (!extension || !extension->is_platform_app()) {
    // Note: web_contents are NULL in unit tests :(
    if (web_contents) {
      args->Append(ExtensionTabUtil::CreateTabValue(web_contents));
    } else {
      args->Append(new base::DictionaryValue());
    }
  }

  if (item->type() == MenuItem::CHECKBOX ||
      item->type() == MenuItem::RADIO) {
    bool was_checked = item->checked();
    properties->SetBoolean("wasChecked", was_checked);

    // RADIO items always get set to true when you click on them, but CHECKBOX
    // items get their state toggled.
    bool checked =
        (item->type() == MenuItem::RADIO) ? true : !was_checked;

    item->SetChecked(checked);
    properties->SetBoolean("checked", item->checked());

    if (extension)
      WriteToStorage(extension, item->id().extension_key);
  }

  // Note: web_contents are NULL in unit tests :(
  if (web_contents && extensions::TabHelper::FromWebContents(web_contents)) {
    extensions::TabHelper::FromWebContents(web_contents)->
        active_tab_permission_granter()->GrantIfRequested(extension);
  }

  {
    // Dispatch to menu item's .onclick handler.
    scoped_ptr<Event> event(
        new Event(webview_guest ? kOnWebviewContextMenus
                                : kOnContextMenus,
                  scoped_ptr<base::ListValue>(args->DeepCopy())));
    event->restrict_to_browser_context = profile;
    event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
    event_router->DispatchEventToExtension(item->extension_id(), event.Pass());
  }
  {
    // Dispatch to .contextMenus.onClicked handler.
    scoped_ptr<Event> event(
        new Event(webview_guest ? web_view::OnClicked::kEventName
                                : context_menus::OnClicked::kEventName,
                  args.Pass()));
    event->restrict_to_browser_context = profile;
    event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
    if (webview_guest)
      event->filter_info.SetInstanceID(webview_guest->view_instance_id());
    event_router->DispatchEventToExtension(item->extension_id(), event.Pass());
  }
}

void MenuManager::SanitizeRadioList(const MenuItem::List& item_list) {
  MenuItem::List::const_iterator i = item_list.begin();
  while (i != item_list.end()) {
    if ((*i)->type() != MenuItem::RADIO) {
      ++i;
      break;
    }

    // Uncheck any checked radio items in the run, and at the end reset
    // the appropriate one to checked. If no check radio items were found,
    // then check the first radio item in the run.
    MenuItem::List::const_iterator last_checked = item_list.end();
    MenuItem::List::const_iterator radio_run_iter;
    for (radio_run_iter = i; radio_run_iter != item_list.end();
        ++radio_run_iter) {
      if ((*radio_run_iter)->type() != MenuItem::RADIO) {
        break;
      }

      if ((*radio_run_iter)->checked()) {
        last_checked = radio_run_iter;
        (*radio_run_iter)->SetChecked(false);
      }
    }

    if (last_checked != item_list.end())
      (*last_checked)->SetChecked(true);
    else
      (*i)->SetChecked(true);

    i = radio_run_iter;
  }
}

bool MenuManager::ItemUpdated(const MenuItem::Id& id) {
  if (!ContainsKey(items_by_id_, id))
    return false;

  MenuItem* menu_item = GetItemById(id);
  DCHECK(menu_item);

  if (menu_item->parent_id()) {
    SanitizeRadioList(GetItemById(*menu_item->parent_id())->children());
  } else {
    MenuItemMap::iterator i =
        context_items_.find(menu_item->id().extension_key);
    if (i == context_items_.end()) {
      NOTREACHED();
      return false;
    }
    SanitizeRadioList(i->second);
  }

  return true;
}

void MenuManager::WriteToStorage(const Extension* extension,
                                 const MenuItem::ExtensionKey& extension_key) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return;
  // <webview> menu items are transient and not stored in storage.
  if (extension_key.webview_instance_id)
    return;
  const MenuItem::List* top_items = MenuItems(extension_key);
  MenuItem::List all_items;
  if (top_items) {
    for (MenuItem::List::const_iterator i = top_items->begin();
         i != top_items->end(); ++i) {
      DCHECK(!(*i)->id().extension_key.webview_instance_id);
      (*i)->GetFlattenedSubtree(&all_items);
    }
  }

  if (store_) {
    store_->SetExtensionValue(extension->id(), kContextMenusKey,
                              MenuItemsToValue(all_items));
  }
}

void MenuManager::ReadFromStorage(const std::string& extension_id,
                                  scoped_ptr<base::Value> value) {
  const Extension* extension =
      ExtensionSystem::Get(profile_)->extension_service()->extensions()->
          GetByID(extension_id);
  if (!extension)
    return;

  MenuItem::List items = MenuItemsFromValue(extension_id, value.get());
  for (size_t i = 0; i < items.size(); ++i) {
    bool added = false;

    if (items[i]->parent_id()) {
      // Parent IDs are stored in the parent_id field for convenience, but
      // they have not yet been validated. Separate them out here.
      // Because of the order in which we store items in the prefs, parents will
      // precede children, so we should already know about any parent items.
      scoped_ptr<MenuItem::Id> parent_id;
      parent_id.swap(items[i]->parent_id_);
      added = AddChildItem(*parent_id, items[i]);
    } else {
      added = AddContextItem(extension, items[i]);
    }

    if (!added)
      delete items[i];
  }
}

void MenuManager::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  if (store_ && BackgroundInfo::HasLazyBackgroundPage(extension)) {
    store_->GetExtensionValue(
        extension->id(),
        kContextMenusKey,
        base::Bind(
            &MenuManager::ReadFromStorage, AsWeakPtr(), extension->id()));
  }
}

void MenuManager::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionInfo::Reason reason) {
  MenuItem::ExtensionKey extension_key(extension->id());
  if (ContainsKey(context_items_, extension_key)) {
    RemoveAllContextItems(extension_key);
  }
}

void MenuManager::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);
  Profile* profile = content::Source<Profile>(source).ptr();
  // We cannot use profile_->HasOffTheRecordProfile as it may already be
  // false at this point, if for example the incognito profile was destroyed
  // using DestroyOffTheRecordProfile.
  if (profile->GetOriginalProfile() == profile_ &&
      profile->GetOriginalProfile() != profile) {
    RemoveAllIncognitoContextItems();
  }
}

const SkBitmap& MenuManager::GetIconForExtension(
    const std::string& extension_id) {
  return icon_manager_.GetIcon(extension_id);
}

void MenuManager::RemoveAllIncognitoContextItems() {
  // Get all context menu items with "incognito" set to "split".
  std::set<MenuItem::Id> items_to_remove;
  std::map<MenuItem::Id, MenuItem*>::const_iterator iter;
  for (iter = items_by_id_.begin();
       iter != items_by_id_.end();
       ++iter) {
    if (iter->first.incognito)
      items_to_remove.insert(iter->first);
  }

  std::set<MenuItem::Id>::iterator remove_iter;
  for (remove_iter = items_to_remove.begin();
       remove_iter != items_to_remove.end();
       ++remove_iter)
    RemoveContextMenuItem(*remove_iter);
}

MenuItem::ExtensionKey::ExtensionKey() : webview_instance_id(0) {}

MenuItem::ExtensionKey::ExtensionKey(const std::string& extension_id,
                                     int webview_instance_id)
    : extension_id(extension_id), webview_instance_id(webview_instance_id) {}

MenuItem::ExtensionKey::ExtensionKey(const std::string& extension_id)
    : extension_id(extension_id), webview_instance_id(0) {}

bool MenuItem::ExtensionKey::operator==(const ExtensionKey& other) const {
  return extension_id == other.extension_id &&
         webview_instance_id == other.webview_instance_id;
}

bool MenuItem::ExtensionKey::operator<(const ExtensionKey& other) const {
  if (extension_id != other.extension_id)
    return extension_id < other.extension_id;

  return webview_instance_id < other.webview_instance_id;
}

bool MenuItem::ExtensionKey::operator!=(const ExtensionKey& other) const {
  return !(*this == other);
}

bool MenuItem::ExtensionKey::empty() const {
  return extension_id.empty() && !webview_instance_id;
}

MenuItem::Id::Id() : incognito(false), uid(0) {}

MenuItem::Id::Id(bool incognito, const MenuItem::ExtensionKey& extension_key)
    : incognito(incognito), extension_key(extension_key), uid(0) {}

MenuItem::Id::~Id() {
}

bool MenuItem::Id::operator==(const Id& other) const {
  return (incognito == other.incognito &&
          extension_key == other.extension_key && uid == other.uid &&
          string_uid == other.string_uid);
}

bool MenuItem::Id::operator!=(const Id& other) const {
  return !(*this == other);
}

bool MenuItem::Id::operator<(const Id& other) const {
  if (incognito < other.incognito)
    return true;
  if (incognito == other.incognito) {
    if (extension_key < other.extension_key)
      return true;
    if (extension_key == other.extension_key) {
      if (uid < other.uid)
        return true;
      if (uid == other.uid)
        return string_uid < other.string_uid;
    }
  }
  return false;
}

}  // namespace extensions
