// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_menu_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/json/json_writer.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"
#include "webkit/glue/context_menu.h"

ExtensionMenuItem::ExtensionMenuItem(const Id& id,
                                     std::string title,
                                     bool checked,
                                     Type type,
                                     const ContextList& contexts)
    : id_(id),
      title_(title),
      type_(type),
      checked_(checked),
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
    result = l10n_util::TruncateString(result, max_length);
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

const int ExtensionMenuManager::kAllowedSchemes =
    URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS;

ExtensionMenuManager::ExtensionMenuManager() {
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
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

bool ExtensionMenuManager::AddContextItem(const Extension* extension,
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

  if (item->type() == ExtensionMenuItem::RADIO && item->checked())
    RadioItemSelected(item);

  // If this is the first item for this extension, start loading its icon.
  if (first_item)
    icon_manager_.LoadIcon(extension);

  return true;
}

bool ExtensionMenuManager::AddChildItem(const ExtensionMenuItem::Id& parent_id,
                                        ExtensionMenuItem* child) {
  ExtensionMenuItem* parent = GetItemById(parent_id);
  if (!parent || parent->type() != ExtensionMenuItem::NORMAL ||
      parent->extension_id() != child->extension_id() ||
      ContainsKey(items_by_id_, child->id()))
    return false;
  parent->AddChild(child);
  items_by_id_[child->id()] = child;
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
  }

  if (new_parent) {
    new_parent->AddChild(child);
  } else {
    context_items_[child->extension_id()].push_back(child);
    child->parent_id_.reset(NULL);
  }
  return true;
}

bool ExtensionMenuManager::RemoveContextMenuItem(
    const ExtensionMenuItem::Id& id) {
  if (!ContainsKey(items_by_id_, id))
    return false;

  std::string extension_id = GetItemById(id)->extension_id();
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
      break;
    } else {
      // See if the item to remove was found as a descendant of the current
      // top-level item.
      ExtensionMenuItem* child = (*j)->ReleaseChild(id, true /* recursive */);
      if (child) {
        items_removed = child->RemoveAllDescendants();
        items_removed.insert(id);
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

void ExtensionMenuManager::RemoveAllContextItems(std::string extension_id) {
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
    TabContents* tab_contents,
    const ContextMenuParams& params,
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
  properties->SetInteger("menuItemId", item->id().uid);
  if (item->parent_id())
    properties->SetInteger("parentMenuItemId", item->parent_id()->uid);

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
  if (tab_contents) {
    args.Append(ExtensionTabUtil::CreateTabValue(tab_contents));
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
  }

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  std::string event_name = "contextMenus";
  event_router->DispatchEventToExtension(
      item->extension_id(), event_name, json_args, profile, GURL());
}

void ExtensionMenuManager::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  // Remove menu items for disabled/uninstalled extensions.
  if (type != NotificationType::EXTENSION_UNLOADED) {
    NOTREACHED();
    return;
  }
  const Extension* extension =
      Details<UnloadedExtensionInfo>(details)->extension;
  if (ContainsKey(context_items_, extension->id())) {
    RemoveAllContextItems(extension->id());
  }
}

const SkBitmap& ExtensionMenuManager::GetIconForExtension(
    const std::string& extension_id) {
  return icon_manager_.GetIcon(extension_id);
}

// static
bool ExtensionMenuManager::HasAllowedScheme(const GURL& url) {
  URLPattern pattern(kAllowedSchemes);
  return pattern.SetScheme(url.scheme());
}

ExtensionMenuItem::Id::Id()
    : profile(NULL), uid(0) {
}

ExtensionMenuItem::Id::Id(Profile* profile, std::string extension_id, int uid)
    : profile(profile), extension_id(extension_id), uid(uid) {
}

ExtensionMenuItem::Id::~Id() {
}

bool ExtensionMenuItem::Id::operator==(const Id& other) const {
  return (profile == other.profile &&
          extension_id == other.extension_id &&
          uid == other.uid);
}

bool ExtensionMenuItem::Id::operator!=(const Id& other) const {
  return !(*this == other);
}

bool ExtensionMenuItem::Id::operator<(const Id& other) const {
  if (profile < other.profile)
    return true;
  if (profile == other.profile) {
    if (extension_id < other.extension_id)
      return true;
    if (extension_id == other.extension_id)
      return uid < other.uid;
  }
  return false;
}
