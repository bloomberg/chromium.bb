// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_toolbar_model.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/browser_event_router.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"

using extensions::Extension;
using extensions::ExtensionList;

namespace {

// Returns true if an |extension| is in an |extension_list|.
bool IsInExtensionList(const Extension* extension,
                       const extensions::ExtensionList& extension_list) {
  for (size_t i = 0; i < extension_list.size(); i++) {
    if (extension_list[i].get() == extension)
      return true;
  }
  return false;
}

}  // namespace

ExtensionToolbarModel::ExtensionToolbarModel(ExtensionService* service)
    : service_(service),
      prefs_(service->profile()->GetPrefs()),
      extensions_initialized_(false) {
  DCHECK(service_);

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(service_->profile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(service_->profile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(service_->profile()));
  registrar_.Add(
      this, chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
      content::Source<extensions::ExtensionPrefs>(service_->extension_prefs()));

  visible_icon_count_ = prefs_->GetInteger(prefs::kExtensionToolbarSize);
}

ExtensionToolbarModel::~ExtensionToolbarModel() {
}

void ExtensionToolbarModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionToolbarModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionToolbarModel::MoveBrowserAction(const Extension* extension,
                                              int index) {
  ExtensionList::iterator pos = std::find(toolbar_items_.begin(),
      toolbar_items_.end(), extension);
  if (pos == toolbar_items_.end()) {
    NOTREACHED();
    return;
  }
  toolbar_items_.erase(pos);

  int i = 0;
  bool inserted = false;
  for (ExtensionList::iterator iter = toolbar_items_.begin();
       iter != toolbar_items_.end();
       ++iter, ++i) {
    if (i == index) {
      toolbar_items_.insert(iter, make_scoped_refptr(extension));
      inserted = true;
      break;
    }
  }

  if (!inserted) {
    DCHECK_EQ(index, static_cast<int>(toolbar_items_.size()));
    index = toolbar_items_.size();

    toolbar_items_.push_back(make_scoped_refptr(extension));
  }

  FOR_EACH_OBSERVER(Observer, observers_, BrowserActionMoved(extension, index));

  UpdatePrefs();
}

ExtensionToolbarModel::Action ExtensionToolbarModel::ExecuteBrowserAction(
    const Extension* extension,
    Browser* browser,
    GURL* popup_url_out) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return ACTION_NONE;

  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id < 0)
    return ACTION_NONE;

  ExtensionAction* browser_action =
      extensions::ExtensionActionManager::Get(service_->profile())->
      GetBrowserAction(*extension);

  // For browser actions, visibility == enabledness.
  if (!browser_action->GetIsVisible(tab_id))
    return ACTION_NONE;

  extensions::TabHelper::FromWebContents(web_contents)->
      active_tab_permission_granter()->GrantIfRequested(extension);

  if (browser_action->HasPopup(tab_id)) {
    if (popup_url_out)
      *popup_url_out = browser_action->GetPopupUrl(tab_id);
    return ACTION_SHOW_POPUP;
  }

  service_->browser_event_router()->BrowserActionExecuted(
      *browser_action, browser);
  return ACTION_NONE;
}

void ExtensionToolbarModel::SetVisibleIconCount(int count) {
  visible_icon_count_ =
      count == static_cast<int>(toolbar_items_.size()) ? -1 : count;
  prefs_->SetInteger(prefs::kExtensionToolbarSize, visible_icon_count_);
}

void ExtensionToolbarModel::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSIONS_READY) {
    InitializeExtensionLists();
    return;
  }

  if (!service_->is_ready())
    return;

  const Extension* extension = NULL;
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    extension = content::Details<extensions::UnloadedExtensionInfo>(
        details)->extension;
  } else {
    extension = content::Details<const Extension>(details).ptr();
  }
  ExtensionList* list_with_extension = FindListWithExtension(extension);
  if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
    // We don't want to add the same extension twice. It may have already been
    // added by EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED below, if the user
    // hides the browser action and then disables and enables the extension.
    if (list_with_extension)
      return;
    if (extensions::FeatureSwitch::extensions_in_action_box()->IsEnabled())
      AddExtension(extension, &action_box_menu_items_);
    else if (service_->extension_prefs()->GetBrowserActionVisibility(extension))
      AddExtension(extension, &toolbar_items_);
  } else if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    if (list_with_extension)
      RemoveExtension(extension, list_with_extension);
  } else if (type ==
      chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED) {
    if (extensions::FeatureSwitch::extensions_in_action_box()->IsEnabled()) {
      // TODO(yefim): Implement this when implementing drag & drop
      // for action box menu.
    } else if (
      service_->extension_prefs()->GetBrowserActionVisibility(extension)) {
      AddExtension(extension, &toolbar_items_);
    } else {
      RemoveExtension(extension, &toolbar_items_);
    }
  } else {
    NOTREACHED() << "Received unexpected notification";
  }
}

void ExtensionToolbarModel::AddExtension(const Extension* extension,
                                         ExtensionList* list) {
  // We only care about extensions with browser actions.
  if (!extensions::ExtensionActionManager::Get(service_->profile())->
      GetBrowserAction(*extension)) {
    return;
  }

  if (extension->id() == last_extension_removed_ &&
      last_extension_removed_index_ < list->size()) {
    list->insert(list->begin() + last_extension_removed_index_,
                 make_scoped_refptr(extension));
    // TODO: figure out the right long term solution.
    if (list == &toolbar_items_) {
      FOR_EACH_OBSERVER(Observer, observers_,
          BrowserActionAdded(extension, last_extension_removed_index_));
    }
  } else {
    list->push_back(make_scoped_refptr(extension));
    // TODO: figure out the right long term solution.
    if (list == &toolbar_items_) {
      FOR_EACH_OBSERVER(Observer, observers_,
                        BrowserActionAdded(extension, list->size() - 1));
    }
  }

  last_extension_removed_ = "";
  last_extension_removed_index_ = -1;

  UpdatePrefs();
}

void ExtensionToolbarModel::RemoveExtension(const Extension* extension,
                                            ExtensionList* list) {
  ExtensionList::iterator pos =
      std::find(list->begin(), list->end(), extension);
  if (pos == list->end())
    return;

  last_extension_removed_ = extension->id();
  last_extension_removed_index_ = pos - list->begin();

  list->erase(pos);
  // TODO: figure out the right long term solution.
  if (list == &toolbar_items_) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      BrowserActionRemoved(extension));
  }

  UpdatePrefs();
}

extensions::ExtensionList* ExtensionToolbarModel::FindListWithExtension(
    const Extension* extension) {
  if (IsInExtensionList(extension, toolbar_items_))
    return &toolbar_items_;
  return IsInExtensionList(extension, action_box_menu_items_) ?
      &action_box_menu_items_ : NULL;
}


// Combine the currently enabled extensions that have browser actions (which
// we get from the ExtensionService) with the ordering we get from the
// pref service. For robustness we use a somewhat inefficient process:
// 1. Create a vector of extensions sorted by their pref values. This vector may
// have holes.
// 2. Create a vector of extensions that did not have a pref value.
// 3. Remove holes from the sorted vector and append the unsorted vector.
void ExtensionToolbarModel::InitializeExtensionLists() {
  DCHECK(service_->is_ready());

  if (extensions::FeatureSwitch::extensions_in_action_box()->IsEnabled())
    PopulateForActionBoxMode();
  else
    PopulateForNonActionBoxMode();

  UpdatePrefs();

  extensions_initialized_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, ModelLoaded());
}

void ExtensionToolbarModel::PopulateForActionBoxMode() {
  const extensions::ExtensionIdList toolbar_order =
      service_->extension_prefs()->GetToolbarOrder();
  extensions::ExtensionIdList action_box_order =
      service_->extension_prefs()->GetActionBoxOrder();

  extensions::ExtensionActionManager* extension_action_manager =
      extensions::ExtensionActionManager::Get(service_->profile());

  // Add all browser actions not already in the toolbar or action box
  // to the action box (the prefs list may omit some extensions).
  for (ExtensionSet::const_iterator iter = service_->extensions()->begin();
       iter != service_->extensions()->end(); ++iter) {
    const Extension* extension = *iter;
    if (!extension_action_manager->GetBrowserAction(*extension))
      continue;

    if (std::find(toolbar_order.begin(), toolbar_order.end(),
                  extension->id()) != toolbar_order.end())
      continue;

    if (std::find(action_box_order.begin(), action_box_order.end(),
                  extension->id()) != action_box_order.end())
      continue;

    action_box_order.push_back(extension->id());
  }

  FillExtensionList(action_box_order, &action_box_menu_items_);
  FillExtensionList(toolbar_order, &toolbar_items_);
}

void ExtensionToolbarModel::PopulateForNonActionBoxMode() {
  const extensions::ExtensionIdList pref_order =
      service_->extension_prefs()->GetToolbarOrder();
  // Items that have a pref for their position.
  ExtensionList sorted;
  sorted.resize(pref_order.size(), NULL);
  // The items that don't have a pref for their position.
  ExtensionList unsorted;

  extensions::ExtensionActionManager* extension_action_manager =
      extensions::ExtensionActionManager::Get(service_->profile());

  // Create the lists.
  for (ExtensionSet::const_iterator it = service_->extensions()->begin();
       it != service_->extensions()->end(); ++it) {
    const Extension* extension = *it;
    if (!extension_action_manager->GetBrowserAction(*extension))
      continue;
    if (!service_->extension_prefs()->GetBrowserActionVisibility(extension))
      continue;

    extensions::ExtensionIdList::const_iterator pos =
        std::find(pref_order.begin(), pref_order.end(), extension->id());
    if (pos != pref_order.end())
      sorted[pos - pref_order.begin()] = extension;
    else
      unsorted.push_back(make_scoped_refptr(extension));
  }

  // Merge the lists.
  toolbar_items_.clear();
  toolbar_items_.reserve(sorted.size() + unsorted.size());
  for (ExtensionList::const_iterator iter = sorted.begin();
       iter != sorted.end(); ++iter) {
    // It's possible for the extension order to contain items that aren't
    // actually loaded on this machine.  For example, when extension sync is on,
    // we sync the extension order as-is but double-check with the user before
    // syncing NPAPI-containing extensions, so if one of those is not actually
    // synced, we'll get a NULL in the list.  This sort of case can also happen
    // if some error prevents an extension from loading.
    if (*iter != NULL)
      toolbar_items_.push_back(*iter);
  }
  toolbar_items_.insert(toolbar_items_.end(), unsorted.begin(),
                        unsorted.end());

  // Inform observers.
  for (size_t i = 0; i < toolbar_items_.size(); i++) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      BrowserActionAdded(toolbar_items_[i], i));
  }
}

void ExtensionToolbarModel::FillExtensionList(
    const extensions::ExtensionIdList& order,
    ExtensionList* list) {
  list->clear();
  list->reserve(order.size());
  for (size_t i = 0; i < order.size(); ++i) {
    const extensions::Extension* extension =
        service_->GetExtensionById(order[i], false);
    if (extension)
      AddExtension(extension, list);
  }
}

void ExtensionToolbarModel::UpdatePrefs() {
  if (!service_->extension_prefs())
    return;

  extensions::ExtensionIdList toolbar_ids;
  toolbar_ids.reserve(toolbar_items_.size());
  for (size_t i = 0; i < toolbar_items_.size(); ++i)
    toolbar_ids.push_back(toolbar_items_[i]->id());
  service_->extension_prefs()->SetToolbarOrder(toolbar_ids);

  extensions::ExtensionIdList action_box_ids;
  action_box_ids.reserve(action_box_menu_items_.size());
  for (size_t i = 0; i < action_box_menu_items_.size(); ++i)
    action_box_ids.push_back(action_box_menu_items_[i]->id());
  service_->extension_prefs()->SetActionBoxOrder(action_box_ids);
}

int ExtensionToolbarModel::IncognitoIndexToOriginal(int incognito_index) {
  int original_index = 0, i = 0;
  for (ExtensionList::iterator iter = toolbar_items_.begin();
       iter != toolbar_items_.end();
       ++iter, ++original_index) {
    if (service_->IsIncognitoEnabled((*iter)->id())) {
      if (incognito_index == i)
        break;
      ++i;
    }
  }
  return original_index;
}

int ExtensionToolbarModel::OriginalIndexToIncognito(int original_index) {
  int incognito_index = 0, i = 0;
  for (ExtensionList::iterator iter = toolbar_items_.begin();
       iter != toolbar_items_.end();
       ++iter, ++i) {
    if (original_index == i)
      break;
    if (service_->IsIncognitoEnabled((*iter)->id()))
      ++incognito_index;
  }
  return incognito_index;
}
