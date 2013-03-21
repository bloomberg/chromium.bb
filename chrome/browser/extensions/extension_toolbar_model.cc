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
using extensions::ExtensionIdList;
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
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
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

  ExtensionIdList::iterator pos_id;
  pos_id = std::find(last_known_positions_.begin(),
                     last_known_positions_.end(), extension->id());
  if (pos_id != last_known_positions_.end())
    last_known_positions_.erase(pos_id);

  int i = 0;
  bool inserted = false;
  for (ExtensionList::iterator iter = toolbar_items_.begin();
       iter != toolbar_items_.end();
       ++iter, ++i) {
    if (i == index) {
      pos_id = std::find(last_known_positions_.begin(),
                         last_known_positions_.end(), (*iter)->id());
      last_known_positions_.insert(pos_id, extension->id());

      toolbar_items_.insert(iter, make_scoped_refptr(extension));
      inserted = true;
      break;
    }
  }

  if (!inserted) {
    DCHECK_EQ(index, static_cast<int>(toolbar_items_.size()));
    index = toolbar_items_.size();

    toolbar_items_.push_back(make_scoped_refptr(extension));
    last_known_positions_.push_back(extension->id());
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
    InitializeExtensionList();
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
  if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
    // We don't want to add the same extension twice. It may have already been
    // added by EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED below, if the user
    // hides the browser action and then disables and enables the extension.
    for (size_t i = 0; i < toolbar_items_.size(); i++) {
      if (toolbar_items_[i].get() == extension)
        return;  // Already exists.
    }
    if (service_->extension_prefs()->GetBrowserActionVisibility(extension))
      AddExtension(extension);
  } else if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    RemoveExtension(extension);
  } else if (type == chrome::NOTIFICATION_EXTENSION_UNINSTALLED) {
    UninstalledExtension(extension);
  } else if (type ==
      chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED) {
    if (service_->extension_prefs()->GetBrowserActionVisibility(extension))
      AddExtension(extension);
    else
      RemoveExtension(extension);
  } else {
    NOTREACHED() << "Received unexpected notification";
  }
}

size_t ExtensionToolbarModel::FindNewPositionFromLastKnownGood(
    const Extension* extension) {
  // See if we have last known good position for this extension.
  size_t new_index = 0;
  // Loop through the ID list of known positions, to count the number of visible
  // browser action icons preceding |extension|.
  for (ExtensionIdList::const_iterator iter_id = last_known_positions_.begin();
       iter_id < last_known_positions_.end(); ++iter_id) {
    if ((*iter_id) == extension->id())
      return new_index;  // We've found the right position.
    // Found an id, need to see if it is visible.
    for (ExtensionList::const_iterator iter_ext = toolbar_items_.begin();
         iter_ext < toolbar_items_.end(); ++iter_ext) {
      if ((*iter_ext)->id().compare(*iter_id) == 0) {
        // This extension is visible, update the index value.
        ++new_index;
        break;
      }
    }
  }

  return -1;
}

void ExtensionToolbarModel::AddExtension(const Extension* extension) {
  // We only care about extensions with browser actions.
  if (!extensions::ExtensionActionManager::Get(service_->profile())->
      GetBrowserAction(*extension)) {
    return;
  }

  size_t new_index = -1;

  // See if we have a last known good position for this extension.
  ExtensionIdList::iterator last_pos = std::find(last_known_positions_.begin(),
                                                 last_known_positions_.end(),
                                                 extension->id());
  if (last_pos != last_known_positions_.end()) {
    new_index = FindNewPositionFromLastKnownGood(extension);
    if (new_index != toolbar_items_.size()) {
      toolbar_items_.insert(toolbar_items_.begin() + new_index,
                            make_scoped_refptr(extension));
    } else {
      toolbar_items_.push_back(make_scoped_refptr(extension));
    }
  } else {
    // This is a never before seen extension, that was added to the end. Make
    // sure to reflect that.
    toolbar_items_.push_back(make_scoped_refptr(extension));
    last_known_positions_.push_back(extension->id());
    new_index = toolbar_items_.size() - 1;
    UpdatePrefs();
  }

  FOR_EACH_OBSERVER(Observer, observers_,
                    BrowserActionAdded(extension, new_index));
}

void ExtensionToolbarModel::RemoveExtension(const Extension* extension) {
  ExtensionList::iterator pos =
      std::find(toolbar_items_.begin(), toolbar_items_.end(), extension);
  if (pos == toolbar_items_.end())
    return;

  toolbar_items_.erase(pos);
  FOR_EACH_OBSERVER(Observer, observers_,
                    BrowserActionRemoved(extension));

  UpdatePrefs();
}

void ExtensionToolbarModel::UninstalledExtension(const Extension* extension) {
  // Remove the extension id from the ordered list, if it exists (the extension
  // might not be represented in the list because it might not have an icon).
  ExtensionIdList::iterator pos =
      std::find(last_known_positions_.begin(),
                last_known_positions_.end(), extension->id());

  if (pos != last_known_positions_.end()) {
    last_known_positions_.erase(pos);
    UpdatePrefs();
  }
}

// Combine the currently enabled extensions that have browser actions (which
// we get from the ExtensionService) with the ordering we get from the
// pref service. For robustness we use a somewhat inefficient process:
// 1. Create a vector of extensions sorted by their pref values. This vector may
// have holes.
// 2. Create a vector of extensions that did not have a pref value.
// 3. Remove holes from the sorted vector and append the unsorted vector.
void ExtensionToolbarModel::InitializeExtensionList() {
  DCHECK(service_->is_ready());

  Populate();
  UpdatePrefs();

  extensions_initialized_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, ModelLoaded());
}

void ExtensionToolbarModel::Populate() {
  const extensions::ExtensionIdList pref_order =
      service_->extension_prefs()->GetToolbarOrder();
  last_known_positions_ = pref_order;

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
    const extensions::ExtensionIdList& order) {
  toolbar_items_.clear();
  toolbar_items_.reserve(order.size());
  for (size_t i = 0; i < order.size(); ++i) {
    const extensions::Extension* extension =
        service_->GetExtensionById(order[i], false);
    if (extension)
      AddExtension(extension);
  }
}

void ExtensionToolbarModel::UpdatePrefs() {
  if (!service_->extension_prefs())
    return;

  service_->extension_prefs()->SetToolbarOrder(last_known_positions_);
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
