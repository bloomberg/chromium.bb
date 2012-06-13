// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_menu_manager.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/context_menu_params.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/favicon_size.h"

using content::WebContents;

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
                   const ExtensionMenuItem::Id& id) {
  if (id.uid == 0)
    properties->SetString(key, id.string_uid);
  else
    properties->SetInteger(key, id.uid);
}

ExtensionMenuItem::List MenuItemsFromValue(const std::string& extension_id,
                                           base::Value* value) {
  ExtensionMenuItem::List items;

  base::ListValue* list = NULL;
  if (!value || !value->GetAsList(&list))
    return items;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* dict = NULL;
    if (!list->GetDictionary(i, &dict))
      continue;
    ExtensionMenuItem* item = ExtensionMenuItem::Populate(
        extension_id, *dict, NULL);
    if (!item)
      continue;
    items.push_back(item);
  }
  return items;
}

scoped_ptr<base::Value> MenuItemsToValue(
    const ExtensionMenuItem::List& items) {
  scoped_ptr<base::ListValue> list(new ListValue());
  for (size_t i = 0; i < items.size(); ++i)
    list->Append(items[i]->ToValue().release());
  return scoped_ptr<Value>(list.release());
}

}  // namespace

ExtensionMenuItem::ExtensionMenuItem(const Id& id,
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
      contexts_(contexts),
      parent_id_(0) {
}

ExtensionMenuItem::~ExtensionMenuItem() {
  STLDeleteElements(&children_);
}

ExtensionMenuItem* ExtensionMenuItem::ReleaseChild(const Id& child_id,
                                                   bool recursive) {
  for (List::iterator i = children_.begin(); i != children_.end(); ++i) {
    ExtensionMenuItem* child = NULL;
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

void ExtensionMenuItem::GetFlattenedSubtree(ExtensionMenuItem::List* list) {
  list->push_back(this);
  for (List::iterator i = children_.begin(); i != children_.end(); ++i)
    (*i)->GetFlattenedSubtree(list);
}

std::set<ExtensionMenuItem::Id> ExtensionMenuItem::RemoveAllDescendants() {
  std::set<Id> result;
  for (List::iterator i = children_.begin(); i != children_.end(); ++i) {
    ExtensionMenuItem* child = *i;
    result.insert(child->id());
    std::set<Id> removed = child->RemoveAllDescendants();
    result.insert(removed.begin(), removed.end());
  }
  STLDeleteElements(&children_);
  return result;
}

string16 ExtensionMenuItem::TitleWithReplacement(
    const string16& selection, size_t max_length) const {
  string16 result = UTF8ToUTF16(title_);
  // TODO(asargent) - Change this to properly handle %% escaping so you can
  // put "%s" in titles that won't get substituted.
  ReplaceSubstringsAfterOffset(&result, 0, ASCIIToUTF16("%s"), selection);

  if (result.length() > max_length)
    result = ui::TruncateString(result, max_length);
  return result;
}

bool ExtensionMenuItem::SetChecked(bool checked) {
  if (type_ != CHECKBOX && type_ != RADIO)
    return false;
  checked_ = checked;
  return true;
}

void ExtensionMenuItem::AddChild(ExtensionMenuItem* item) {
  item->parent_id_.reset(new Id(id_));
  children_.push_back(item);
}

scoped_ptr<DictionaryValue> ExtensionMenuItem::ToValue() const {
  scoped_ptr<DictionaryValue> value(new DictionaryValue);
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
  if (parent_id_.get()) {
    DCHECK_EQ(0, parent_id_->uid);
    value->SetString(kParentUIDKey, parent_id_->string_uid);
  }
  value->Set(kDocumentURLPatternsKey,
             document_url_patterns_.ToValue().release());
  value->Set(kTargetURLPatternsKey, target_url_patterns_.ToValue().release());
  return value.Pass();
}

// static
ExtensionMenuItem* ExtensionMenuItem::Populate(const std::string& extension_id,
                                               const DictionaryValue& value,
                                               std::string* error) {
  bool incognito = false;
  if (!value.GetBoolean(kIncognitoKey, &incognito))
    return NULL;
  Id id(incognito, extension_id);
  if (!value.GetString(kStringUIDKey, &id.string_uid))
    return NULL;
  int type_int;
  Type type;
  if (!value.GetInteger(kTypeKey, &type_int))
    return NULL;
  type = static_cast<Type>(type_int);
  std::string title;
  if (type != SEPARATOR && !value.GetString(kTitleKey, &title))
    return NULL;
  bool checked;
  if ((type == CHECKBOX || type == RADIO) &&
      !value.GetBoolean(kCheckedKey, &checked)) {
    return NULL;
  }
  bool enabled = true;
  if (!value.GetBoolean(kEnabledKey, &enabled))
    return NULL;
  ContextList contexts;
  Value* contexts_value = NULL;
  if (!value.Get(kContextsKey, &contexts_value))
    return NULL;
  if (!contexts.Populate(*contexts_value))
    return NULL;

  scoped_ptr<ExtensionMenuItem> result(new ExtensionMenuItem(
      id, title, checked, enabled, type, contexts));

  if (!result->PopulateURLPatterns(
          value, kDocumentURLPatternsKey, kTargetURLPatternsKey, error))
    return NULL;

  // parent_id is filled in from the value, but it might not be valid. It's left
  // to be validated upon being added (via AddChildItem) to the menu manager.
  scoped_ptr<Id> parent_id(new Id(incognito, extension_id));
  if (value.HasKey(kParentUIDKey)) {
    if (!value.GetString(kParentUIDKey, &parent_id->string_uid))
      return NULL;
    result->parent_id_.swap(parent_id);
  }
  return result.release();
}

bool ExtensionMenuItem::PopulateURLPatterns(
    const DictionaryValue& properties,
    const char* document_url_patterns_key,
    const char* target_url_patterns_key,
    std::string* error) {
  if (properties.HasKey(document_url_patterns_key)) {
    ListValue* list = NULL;
    if (!properties.GetList(document_url_patterns_key, &list))
      return false;
    if (!document_url_patterns_.Populate(
            *list, URLPattern::SCHEME_ALL, true, error)) {
      return false;
    }
  }
  if (properties.HasKey(target_url_patterns_key)) {
    ListValue* list = NULL;
    if (!properties.GetList(target_url_patterns_key, &list))
      return false;
    if (!target_url_patterns_.Populate(
            *list, URLPattern::SCHEME_ALL, true, error)) {
      return false;
    }
  }
  return true;
}

ExtensionMenuManager::ExtensionMenuManager(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));

  extensions::StateStore* store = ExtensionSystem::Get(profile_)->state_store();
  if (store)
    store->RegisterKey(kContextMenusKey);
}

ExtensionMenuManager::~ExtensionMenuManager() {
  MenuItemMap::iterator i;
  for (i = context_items_.begin(); i != context_items_.end(); ++i) {
    STLDeleteElements(&(i->second));
  }
}

std::set<std::string> ExtensionMenuManager::ExtensionIds() {
  std::set<std::string> id_set;
  for (MenuItemMap::const_iterator i = context_items_.begin();
       i != context_items_.end(); ++i) {
    id_set.insert(i->first);
  }
  return id_set;
}

const ExtensionMenuItem::List* ExtensionMenuManager::MenuItems(
    const std::string& extension_id) {
  MenuItemMap::iterator i = context_items_.find(extension_id);
  if (i != context_items_.end()) {
    return &(i->second);
  }
  return NULL;
}

bool ExtensionMenuManager::AddContextItem(
    const extensions::Extension* extension,
    ExtensionMenuItem* item) {
  const std::string& extension_id = item->extension_id();
  // The item must have a non-empty extension id, and not have already been
  // added.
  if (extension_id.empty() || ContainsKey(items_by_id_, item->id()))
    return false;

  DCHECK_EQ(extension->id(), extension_id);

  bool first_item = !ContainsKey(context_items_, extension_id);
  context_items_[extension_id].push_back(item);
  items_by_id_[item->id()] = item;

  if (item->type() == ExtensionMenuItem::RADIO) {
    if (item->checked())
      RadioItemSelected(item);
    else
      SanitizeRadioList(context_items_[extension_id]);
  }

  // If this is the first item for this extension, start loading its icon.
  if (first_item)
    icon_manager_.LoadIcon(extension);

  return true;
}

bool ExtensionMenuManager::AddChildItem(const ExtensionMenuItem::Id& parent_id,
                                        ExtensionMenuItem* child) {
  ExtensionMenuItem* parent = GetItemById(parent_id);
  if (!parent || parent->type() != ExtensionMenuItem::NORMAL ||
      parent->incognito() != child->incognito() ||
      parent->extension_id() != child->extension_id() ||
      ContainsKey(items_by_id_, child->id()))
    return false;
  parent->AddChild(child);
  items_by_id_[child->id()] = child;

  if (child->type() == ExtensionMenuItem::RADIO)
    SanitizeRadioList(parent->children());
  return true;
}

bool ExtensionMenuManager::DescendantOf(
    ExtensionMenuItem* item,
    const ExtensionMenuItem::Id& ancestor_id) {
  // Work our way up the tree until we find the ancestor or NULL.
  ExtensionMenuItem::Id* id = item->parent_id();
  while (id != NULL) {
    DCHECK(*id != item->id());  // Catch circular graphs.
    if (*id == ancestor_id)
      return true;
    ExtensionMenuItem* next = GetItemById(*id);
    if (!next) {
      NOTREACHED();
      return false;
    }
    id = next->parent_id();
  }
  return false;
}

bool ExtensionMenuManager::ChangeParent(
    const ExtensionMenuItem::Id& child_id,
    const ExtensionMenuItem::Id* parent_id) {
  ExtensionMenuItem* child = GetItemById(child_id);
  ExtensionMenuItem* new_parent = parent_id ? GetItemById(*parent_id) : NULL;
  if ((parent_id && (child_id == *parent_id)) || !child ||
      (!new_parent && parent_id != NULL) ||
      (new_parent && (DescendantOf(new_parent, child_id) ||
                      child->incognito() != new_parent->incognito() ||
                      child->extension_id() != new_parent->extension_id())))
    return false;

  ExtensionMenuItem::Id* old_parent_id = child->parent_id();
  if (old_parent_id != NULL) {
    ExtensionMenuItem* old_parent = GetItemById(*old_parent_id);
    if (!old_parent) {
      NOTREACHED();
      return false;
    }
    ExtensionMenuItem* taken =
      old_parent->ReleaseChild(child_id, false /* non-recursive search*/);
    DCHECK(taken == child);
    SanitizeRadioList(old_parent->children());
  } else {
    // This is a top-level item, so we need to pull it out of our list of
    // top-level items.
    MenuItemMap::iterator i = context_items_.find(child->extension_id());
    if (i == context_items_.end()) {
      NOTREACHED();
      return false;
    }
    ExtensionMenuItem::List& list = i->second;
    ExtensionMenuItem::List::iterator j = std::find(list.begin(), list.end(),
                                                    child);
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
    context_items_[child->extension_id()].push_back(child);
    child->parent_id_.reset(NULL);
    SanitizeRadioList(context_items_[child->extension_id()]);
  }
  return true;
}

bool ExtensionMenuManager::RemoveContextMenuItem(
    const ExtensionMenuItem::Id& id) {
  if (!ContainsKey(items_by_id_, id))
    return false;

  ExtensionMenuItem* menu_item = GetItemById(id);
  DCHECK(menu_item);
  std::string extension_id = menu_item->extension_id();
  MenuItemMap::iterator i = context_items_.find(extension_id);
  if (i == context_items_.end()) {
    NOTREACHED();
    return false;
  }

  bool result = false;
  std::set<ExtensionMenuItem::Id> items_removed;
  ExtensionMenuItem::List& list = i->second;
  ExtensionMenuItem::List::iterator j;
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
      ExtensionMenuItem* child = (*j)->ReleaseChild(id, true /* recursive */);
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
  std::set<ExtensionMenuItem::Id>::iterator removed_iter;
  for (removed_iter = items_removed.begin();
       removed_iter != items_removed.end();
       ++removed_iter) {
    items_by_id_.erase(*removed_iter);
  }

  if (list.empty()) {
    context_items_.erase(extension_id);
    icon_manager_.RemoveIcon(extension_id);
  }
  return result;
}

void ExtensionMenuManager::RemoveAllContextItems(
    const std::string& extension_id) {
  ExtensionMenuItem::List::iterator i;
  for (i = context_items_[extension_id].begin();
       i != context_items_[extension_id].end(); ++i) {
    ExtensionMenuItem* item = *i;
    items_by_id_.erase(item->id());

    // Remove descendants from this item and erase them from the lookup cache.
    std::set<ExtensionMenuItem::Id> removed_ids = item->RemoveAllDescendants();
    std::set<ExtensionMenuItem::Id>::const_iterator j;
    for (j = removed_ids.begin(); j != removed_ids.end(); ++j) {
      items_by_id_.erase(*j);
    }
  }
  STLDeleteElements(&context_items_[extension_id]);
  context_items_.erase(extension_id);
  icon_manager_.RemoveIcon(extension_id);
}

ExtensionMenuItem* ExtensionMenuManager::GetItemById(
    const ExtensionMenuItem::Id& id) const {
  std::map<ExtensionMenuItem::Id, ExtensionMenuItem*>::const_iterator i =
      items_by_id_.find(id);
  if (i != items_by_id_.end())
    return i->second;
  else
    return NULL;
}

void ExtensionMenuManager::RadioItemSelected(ExtensionMenuItem* item) {
  // If this is a child item, we need to get a handle to the list from its
  // parent. Otherwise get a handle to the top-level list.
  const ExtensionMenuItem::List* list = NULL;
  if (item->parent_id()) {
    ExtensionMenuItem* parent = GetItemById(*item->parent_id());
    if (!parent) {
      NOTREACHED();
      return;
    }
    list = &(parent->children());
  } else {
    if (context_items_.find(item->extension_id()) == context_items_.end()) {
      NOTREACHED();
      return;
    }
    list = &context_items_[item->extension_id()];
  }

  // Find where |item| is in the list.
  ExtensionMenuItem::List::const_iterator item_location;
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
  ExtensionMenuItem::List::const_iterator i;
  if (item_location != list->begin()) {
    i = item_location;
    do {
      --i;
      if ((*i)->type() != ExtensionMenuItem::RADIO)
        break;
      (*i)->SetChecked(false);
    } while (i != list->begin());
  }

  // Now iterate forwards from |item| and uncheck any adjacent radio items.
  for (i = item_location + 1; i != list->end(); ++i) {
    if ((*i)->type() != ExtensionMenuItem::RADIO)
      break;
    (*i)->SetChecked(false);
  }
}

static void AddURLProperty(DictionaryValue* dictionary,
                           const std::string& key, const GURL& url) {
  if (!url.is_empty())
    dictionary->SetString(key, url.possibly_invalid_spec());
}

void ExtensionMenuManager::ExecuteCommand(
    Profile* profile,
    WebContents* web_contents,
    const content::ContextMenuParams& params,
    const ExtensionMenuItem::Id& menuItemId) {
  ExtensionEventRouter* event_router = profile->GetExtensionEventRouter();
  if (!event_router)
    return;

  ExtensionMenuItem* item = GetItemById(menuItemId);
  if (!item)
    return;

  if (item->type() == ExtensionMenuItem::RADIO)
    RadioItemSelected(item);

  ListValue args;

  DictionaryValue* properties = new DictionaryValue();
  SetIdKeyValue(properties, "menuItemId", item->id());
  if (item->parent_id())
    SetIdKeyValue(properties, "parentMenuItemId", item->id());

  switch (params.media_type) {
    case WebKit::WebContextMenuData::MediaTypeImage:
      properties->SetString("mediaType", "image");
      break;
    case WebKit::WebContextMenuData::MediaTypeVideo:
      properties->SetString("mediaType", "video");
      break;
    case WebKit::WebContextMenuData::MediaTypeAudio:
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

  args.Append(properties);

  // Add the tab info to the argument list.
  if (web_contents) {
    args.Append(ExtensionTabUtil::CreateTabValue(web_contents));
  } else {
    args.Append(new DictionaryValue());
  }

  if (item->type() == ExtensionMenuItem::CHECKBOX ||
      item->type() == ExtensionMenuItem::RADIO) {
    bool was_checked = item->checked();
    properties->SetBoolean("wasChecked", was_checked);

    // RADIO items always get set to true when you click on them, but CHECKBOX
    // items get their state toggled.
    bool checked =
        (item->type() == ExtensionMenuItem::RADIO) ? true : !was_checked;

    item->SetChecked(checked);
    properties->SetBoolean("checked", item->checked());

    const extensions::Extension* extension = ExtensionSystem::Get(profile_)->
        extension_service()->GetExtensionById(menuItemId.extension_id, false);
    WriteToStorage(extension);
  }

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  event_router->DispatchEventToExtension(
      item->extension_id(), extension_event_names::kOnContextMenus,
      json_args, profile, GURL(),
      ExtensionEventRouter::USER_GESTURE_ENABLED);
  event_router->DispatchEventToExtension(
      item->extension_id(), extension_event_names::kOnContextMenuClicked,
      json_args, profile, GURL(),
      ExtensionEventRouter::USER_GESTURE_ENABLED);
}

void ExtensionMenuManager::SanitizeRadioList(
    const ExtensionMenuItem::List& item_list) {
  ExtensionMenuItem::List::const_iterator i = item_list.begin();
  while (i != item_list.end()) {
    if ((*i)->type() != ExtensionMenuItem::RADIO) {
      ++i;
      break;
    }

    // Uncheck any checked radio items in the run, and at the end reset
    // the appropriate one to checked. If no check radio items were found,
    // then check the first radio item in the run.
    ExtensionMenuItem::List::const_iterator last_checked = item_list.end();
    ExtensionMenuItem::List::const_iterator radio_run_iter;
    for (radio_run_iter = i; radio_run_iter != item_list.end();
        ++radio_run_iter) {
      if ((*radio_run_iter)->type() != ExtensionMenuItem::RADIO) {
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

bool ExtensionMenuManager::ItemUpdated(const ExtensionMenuItem::Id& id) {
  if (!ContainsKey(items_by_id_, id))
    return false;

  ExtensionMenuItem* menu_item = GetItemById(id);
  DCHECK(menu_item);

  if (menu_item->parent_id()) {
    SanitizeRadioList(GetItemById(*menu_item->parent_id())->children());
  } else {
    std::string extension_id = menu_item->extension_id();
    MenuItemMap::iterator i = context_items_.find(extension_id);
    if (i == context_items_.end()) {
      NOTREACHED();
      return false;
    }
    SanitizeRadioList(i->second);
  }

  return true;
}

void ExtensionMenuManager::WriteToStorage(
    const extensions::Extension* extension) {
  if (!extension->has_lazy_background_page())
    return;
  const ExtensionMenuItem::List* top_items = MenuItems(extension->id());
  ExtensionMenuItem::List all_items;
  if (top_items) {
    for (ExtensionMenuItem::List::const_iterator i = top_items->begin();
         i != top_items->end(); ++i) {
      (*i)->GetFlattenedSubtree(&all_items);
    }
  }

  extensions::StateStore* store = ExtensionSystem::Get(profile_)->state_store();
  if (store)
    store->SetExtensionValue(extension->id(), kContextMenusKey,
                             MenuItemsToValue(all_items));
}

void ExtensionMenuManager::ReadFromStorage(const std::string& extension_id,
                                           scoped_ptr<base::Value> value) {
  const extensions::Extension* extension =
      ExtensionSystem::Get(profile_)->extension_service()->extensions()->
          GetByID(extension_id);
  if (!extension)
    return;

  ExtensionMenuItem::List items = MenuItemsFromValue(extension_id, value.get());
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i]->parent_id()) {
      // Parent IDs are stored in the parent_id field for convenience, but
      // they have not yet been validated. Separate them out here.
      // Because of the order in which we store items in the prefs, parents will
      // precede children, so we should already know about any parent items.
      scoped_ptr<ExtensionMenuItem::Id> parent_id;
      parent_id.swap(items[i]->parent_id_);
      AddChildItem(*parent_id, items[i]);
    } else {
      AddContextItem(extension, items[i]);
    }
  }
}

void ExtensionMenuManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    // Remove menu items for disabled/uninstalled extensions.
    const extensions::Extension* extension =
        content::Details<extensions::UnloadedExtensionInfo>(details)->extension;
    if (ContainsKey(context_items_, extension->id())) {
      RemoveAllContextItems(extension->id());
    }
  } else if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
    const extensions::Extension* extension =
        content::Details<const extensions::Extension>(details).ptr();
    extensions::StateStore* store =
        ExtensionSystem::Get(profile_)->state_store();
    if (store && extension->has_lazy_background_page()) {
      store->GetExtensionValue(extension->id(), kContextMenusKey,
          base::Bind(&ExtensionMenuManager::ReadFromStorage,
                     AsWeakPtr(), extension->id()));
    }
  }
}

const SkBitmap& ExtensionMenuManager::GetIconForExtension(
    const std::string& extension_id) {
  return icon_manager_.GetIcon(extension_id);
}

ExtensionMenuItem::Id::Id()
    : incognito(false), extension_id(""), uid(0), string_uid("") {
}

ExtensionMenuItem::Id::Id(bool incognito, const std::string& extension_id)
    : incognito(incognito), extension_id(extension_id), uid(0),
      string_uid("") {
}

ExtensionMenuItem::Id::~Id() {
}

bool ExtensionMenuItem::Id::operator==(const Id& other) const {
  return (incognito == other.incognito &&
          extension_id == other.extension_id &&
          uid == other.uid &&
          string_uid == other.string_uid);
}

bool ExtensionMenuItem::Id::operator!=(const Id& other) const {
  return !(*this == other);
}

bool ExtensionMenuItem::Id::operator<(const Id& other) const {
  if (incognito < other.incognito)
    return true;
  if (incognito == other.incognito) {
    if (extension_id < other.extension_id)
      return true;
    if (extension_id == other.extension_id) {
      if (uid < other.uid)
        return true;
      if (uid == other.uid)
        return string_uid < other.string_uid;
    }
  }
  return false;
}
