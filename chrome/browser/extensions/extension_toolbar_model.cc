// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_toolbar_model.h"

#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_toolbar_model_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/one_shot_event.h"

namespace extensions {

bool ExtensionToolbarModel::Observer::BrowserActionShowPopup(
    const Extension* extension) {
  return false;
}

ExtensionToolbarModel::ExtensionToolbarModel(Profile* profile,
                                             ExtensionPrefs* extension_prefs)
    : profile_(profile),
      extension_prefs_(extension_prefs),
      prefs_(profile_->GetPrefs()),
      extensions_initialized_(false),
      is_highlighting_(false),
      extension_registry_observer_(this),
      weak_ptr_factory_(this) {
  ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE,
      base::Bind(&ExtensionToolbarModel::OnReady,
                 weak_ptr_factory_.GetWeakPtr()));
  visible_icon_count_ = prefs_->GetInteger(pref_names::kToolbarSize);
  pref_change_registrar_.Init(prefs_);
  pref_change_callback_ =
      base::Bind(&ExtensionToolbarModel::OnExtensionToolbarPrefChange,
                 base::Unretained(this));
  pref_change_registrar_.Add(pref_names::kToolbar, pref_change_callback_);
}

ExtensionToolbarModel::~ExtensionToolbarModel() {
}

// static
ExtensionToolbarModel* ExtensionToolbarModel::Get(Profile* profile) {
  return ExtensionToolbarModelFactory::GetForProfile(profile);
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
    GURL* popup_url_out,
    bool should_grant) {
  content::WebContents* web_contents = NULL;
  int tab_id = 0;
  if (!ExtensionTabUtil::GetDefaultTab(browser, &web_contents, &tab_id)) {
    return ACTION_NONE;
  }

  ExtensionAction* browser_action =
      ExtensionActionManager::Get(profile_)->GetBrowserAction(*extension);

  // For browser actions, visibility == enabledness.
  if (!browser_action->GetIsVisible(tab_id))
    return ACTION_NONE;

  if (should_grant) {
    TabHelper::FromWebContents(web_contents)
        ->active_tab_permission_granter()
        ->GrantIfRequested(extension);
  }

  if (browser_action->HasPopup(tab_id)) {
    if (popup_url_out)
      *popup_url_out = browser_action->GetPopupUrl(tab_id);
    return ACTION_SHOW_POPUP;
  }

  ExtensionActionAPI::BrowserActionExecuted(
      browser->profile(), *browser_action, web_contents);
  return ACTION_NONE;
}

void ExtensionToolbarModel::SetVisibleIconCount(int count) {
  visible_icon_count_ =
      count == static_cast<int>(toolbar_items_.size()) ? -1 : count;
  // Only set the prefs if we're not in highlight mode. Highlight mode is
  // designed to be a transitory state, and should not persist across browser
  // restarts (though it may be re-entered).
  if (!is_highlighting_)
    prefs_->SetInteger(pref_names::kToolbarSize, visible_icon_count_);
}

void ExtensionToolbarModel::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  // We don't want to add the same extension twice. It may have already been
  // added by EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED below, if the user
  // hides the browser action and then disables and enables the extension.
  for (size_t i = 0; i < toolbar_items_.size(); i++) {
    if (toolbar_items_[i].get() == extension)
      return;
  }
  if (ExtensionActionAPI::GetBrowserActionVisibility(extension_prefs_,
                                                     extension->id())) {
    AddExtension(extension);
  }
}

void ExtensionToolbarModel::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  RemoveExtension(extension);
}

void ExtensionToolbarModel::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
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

void ExtensionToolbarModel::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
            type);
  const Extension* extension =
      ExtensionRegistry::Get(profile_)->GetExtensionById(
          *content::Details<const std::string>(details).ptr(),
          ExtensionRegistry::EVERYTHING);
  if (ExtensionActionAPI::GetBrowserActionVisibility(extension_prefs_,
                                                     extension->id()))
    AddExtension(extension);
  else
    RemoveExtension(extension);
}

void ExtensionToolbarModel::OnReady() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  InitializeExtensionList(registry->enabled_extensions());
  // Wait until the extension system is ready before observing any further
  // changes so that the toolbar buttons can be shown in their stable ordering
  // taken from prefs.
  extension_registry_observer_.Add(registry);
  registrar_.Add(
      this,
      chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
      content::Source<ExtensionPrefs>(extension_prefs_));
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

  // Position not found.
  return toolbar_items_.size();
}

void ExtensionToolbarModel::AddExtension(const Extension* extension) {
  // We only care about extensions with browser actions.
  if (!ExtensionActionManager::Get(profile_)->GetBrowserAction(*extension))
    return;

  size_t new_index = toolbar_items_.size();

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
    // sure to reflect that. (|new_index| was set above.)
    toolbar_items_.push_back(make_scoped_refptr(extension));
    last_known_positions_.push_back(extension->id());
    UpdatePrefs();
  }

  // If we're currently highlighting, then even though we add a browser action
  // to the full list (|toolbar_items_|, there won't be another *visible*
  // browser action, which was what the observers care about.
  if (!is_highlighting_) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      BrowserActionAdded(extension, new_index));
  }
}

void ExtensionToolbarModel::RemoveExtension(const Extension* extension) {
  ExtensionList::iterator pos =
      std::find(toolbar_items_.begin(), toolbar_items_.end(), extension);
  if (pos == toolbar_items_.end())
    return;

  toolbar_items_.erase(pos);

  // If we're in highlight mode, we also have to remove the extension from
  // the highlighted list.
  if (is_highlighting_) {
    pos = std::find(highlighted_items_.begin(),
                    highlighted_items_.end(),
                    extension);
    if (pos != highlighted_items_.end()) {
      highlighted_items_.erase(pos);
      FOR_EACH_OBSERVER(Observer, observers_, BrowserActionRemoved(extension));
      // If the highlighted list is now empty, we stop highlighting.
      if (highlighted_items_.empty())
        StopHighlighting();
    }
  } else {
    FOR_EACH_OBSERVER(Observer, observers_, BrowserActionRemoved(extension));
  }

  UpdatePrefs();
}

// Combine the currently enabled extensions that have browser actions (which
// we get from the ExtensionRegistry) with the ordering we get from the
// pref service. For robustness we use a somewhat inefficient process:
// 1. Create a vector of extensions sorted by their pref values. This vector may
// have holes.
// 2. Create a vector of extensions that did not have a pref value.
// 3. Remove holes from the sorted vector and append the unsorted vector.
void ExtensionToolbarModel::InitializeExtensionList(
    const ExtensionSet& extensions) {
  last_known_positions_ = extension_prefs_->GetToolbarOrder();
  Populate(last_known_positions_, extensions);

  extensions_initialized_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, VisibleCountChanged());
}

void ExtensionToolbarModel::Populate(const ExtensionIdList& positions,
                                     const ExtensionSet& extensions) {
  // Items that have explicit positions.
  ExtensionList sorted;
  sorted.resize(positions.size(), NULL);
  // The items that don't have explicit positions.
  ExtensionList unsorted;

  ExtensionActionManager* extension_action_manager =
      ExtensionActionManager::Get(profile_);

  // Create the lists.
  int hidden = 0;
  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end();
       ++it) {
    const Extension* extension = it->get();
    if (!extension_action_manager->GetBrowserAction(*extension))
      continue;
    if (!ExtensionActionAPI::GetBrowserActionVisibility(
            extension_prefs_, extension->id())) {
      ++hidden;
      continue;
    }

    ExtensionIdList::const_iterator pos =
        std::find(positions.begin(), positions.end(), extension->id());
    if (pos != positions.end())
      sorted[pos - positions.begin()] = extension;
    else
      unsorted.push_back(make_scoped_refptr(extension));
  }

  size_t items_count = toolbar_items_.size();
  for (size_t i = 0; i < items_count; i++) {
    const Extension* extension = toolbar_items_.back();
    // By popping the extension here (before calling BrowserActionRemoved),
    // we will not shrink visible count by one after BrowserActionRemoved
    // calls SetVisibleCount.
    toolbar_items_.pop_back();
    FOR_EACH_OBSERVER(
        Observer, observers_, BrowserActionRemoved(extension));
  }
  DCHECK(toolbar_items_.empty());

  // Merge the lists.
  toolbar_items_.reserve(sorted.size() + unsorted.size());

  for (ExtensionList::const_iterator iter = sorted.begin();
       iter != sorted.end(); ++iter) {
    // It's possible for the extension order to contain items that aren't
    // actually loaded on this machine.  For example, when extension sync is on,
    // we sync the extension order as-is but double-check with the user before
    // syncing NPAPI-containing extensions, so if one of those is not actually
    // synced, we'll get a NULL in the list.  This sort of case can also happen
    // if some error prevents an extension from loading.
    if (iter->get() != NULL) {
      toolbar_items_.push_back(*iter);
      FOR_EACH_OBSERVER(
          Observer, observers_, BrowserActionAdded(
              *iter, toolbar_items_.size() - 1));
    }
  }
  for (ExtensionList::const_iterator iter = unsorted.begin();
       iter != unsorted.end(); ++iter) {
    if (iter->get() != NULL) {
      toolbar_items_.push_back(*iter);
      FOR_EACH_OBSERVER(
          Observer, observers_, BrowserActionAdded(
              *iter, toolbar_items_.size() - 1));
    }
  }

  UMA_HISTOGRAM_COUNTS_100(
      "ExtensionToolbarModel.BrowserActionsPermanentlyHidden", hidden);
  UMA_HISTOGRAM_COUNTS_100("ExtensionToolbarModel.BrowserActionsCount",
                           toolbar_items_.size());

  if (!toolbar_items_.empty()) {
    // Visible count can be -1, meaning: 'show all'. Since UMA converts negative
    // values to 0, this would be counted as 'show none' unless we convert it to
    // max.
    UMA_HISTOGRAM_COUNTS_100("ExtensionToolbarModel.BrowserActionsVisible",
                             visible_icon_count_ == -1 ?
                                 base::HistogramBase::kSampleType_MAX :
                                 visible_icon_count_);
  }
}

void ExtensionToolbarModel::UpdatePrefs() {
  if (!extension_prefs_)
    return;

  // Don't observe change caused by self.
  pref_change_registrar_.Remove(pref_names::kToolbar);
  extension_prefs_->SetToolbarOrder(last_known_positions_);
  pref_change_registrar_.Add(pref_names::kToolbar, pref_change_callback_);
}

int ExtensionToolbarModel::IncognitoIndexToOriginal(int incognito_index) {
  int original_index = 0, i = 0;
  for (ExtensionList::iterator iter = toolbar_items_.begin();
       iter != toolbar_items_.end();
       ++iter, ++original_index) {
    if (util::IsIncognitoEnabled((*iter)->id(), profile_)) {
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
    if (util::IsIncognitoEnabled((*iter)->id(), profile_))
      ++incognito_index;
  }
  return incognito_index;
}

void ExtensionToolbarModel::OnExtensionToolbarPrefChange() {
  // If extensions are not ready, defer to later Populate() call.
  if (!extensions_initialized_)
    return;

  // Recalculate |last_known_positions_| to be |pref_positions| followed by
  // ones that are only in |last_known_positions_|.
  ExtensionIdList pref_positions = extension_prefs_->GetToolbarOrder();
  size_t pref_position_size = pref_positions.size();
  for (size_t i = 0; i < last_known_positions_.size(); ++i) {
    if (std::find(pref_positions.begin(), pref_positions.end(),
                  last_known_positions_[i]) == pref_positions.end()) {
      pref_positions.push_back(last_known_positions_[i]);
    }
  }
  last_known_positions_.swap(pref_positions);

  // Re-populate.
  Populate(last_known_positions_,
           ExtensionRegistry::Get(profile_)->enabled_extensions());

  if (last_known_positions_.size() > pref_position_size) {
    // Need to update pref because we have extra icons. But can't call
    // UpdatePrefs() directly within observation closure.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionToolbarModel::UpdatePrefs,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

bool ExtensionToolbarModel::ShowBrowserActionPopup(const Extension* extension) {
  ObserverListBase<Observer>::Iterator it(observers_);
  Observer* obs = NULL;
  while ((obs = it.GetNext()) != NULL) {
    // Stop after first popup since it should only show in the active window.
    if (obs->BrowserActionShowPopup(extension))
      return true;
  }
  return false;
}

void ExtensionToolbarModel::EnsureVisibility(
    const ExtensionIdList& extension_ids) {
  if (visible_icon_count_ == -1)
    return;  // Already showing all.

  // Otherwise, make sure we have enough room to show all the extensions
  // requested.
  if (visible_icon_count_ < static_cast<int>(extension_ids.size())) {
    SetVisibleIconCount(extension_ids.size());

    // Inform observers.
    FOR_EACH_OBSERVER(Observer, observers_, VisibleCountChanged());
  }

  if (visible_icon_count_ == -1)
    return;  // May have been set to max by SetVisibleIconCount.

  // Guillotine's Delight: Move an orange noble to the front of the line.
  for (ExtensionIdList::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    for (ExtensionList::const_iterator extension = toolbar_items_.begin();
         extension != toolbar_items_.end(); ++extension) {
      if ((*extension)->id() == (*it)) {
        if (extension - toolbar_items_.begin() >= visible_icon_count_)
          MoveBrowserAction(*extension, 0);
        break;
      }
    }
  }
}

bool ExtensionToolbarModel::HighlightExtensions(
    const ExtensionIdList& extension_ids) {
  highlighted_items_.clear();

  for (ExtensionIdList::const_iterator id = extension_ids.begin();
       id != extension_ids.end();
       ++id) {
    for (ExtensionList::const_iterator extension = toolbar_items_.begin();
         extension != toolbar_items_.end();
         ++extension) {
      if (*id == (*extension)->id())
        highlighted_items_.push_back(*extension);
    }
  }

  // If we have any items in |highlighted_items_|, then we entered highlighting
  // mode.
  if (highlighted_items_.size()) {
    old_visible_icon_count_ = visible_icon_count_;
    is_highlighting_ = true;
    if (visible_icon_count_ != -1 &&
        visible_icon_count_ < static_cast<int>(extension_ids.size())) {
      SetVisibleIconCount(extension_ids.size());
      FOR_EACH_OBSERVER(Observer, observers_, VisibleCountChanged());
    }

    FOR_EACH_OBSERVER(Observer, observers_, HighlightModeChanged(true));
    return true;
  }

  // Otherwise, we didn't enter highlighting mode (and, in fact, exited it if
  // we were otherwise in it).
  if (is_highlighting_)
    StopHighlighting();
  return false;
}

void ExtensionToolbarModel::StopHighlighting() {
  if (is_highlighting_) {
    highlighted_items_.clear();
    is_highlighting_ = false;
    if (old_visible_icon_count_ != visible_icon_count_) {
      SetVisibleIconCount(old_visible_icon_count_);
      FOR_EACH_OBSERVER(Observer, observers_, VisibleCountChanged());
    }
    FOR_EACH_OBSERVER(Observer, observers_, HighlightModeChanged(false));
  }
}

}  // namespace extensions
