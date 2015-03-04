// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_toolbar_model.h"

#include <algorithm>
#include <string>

#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/prefs/pref_service.h"
#include "base/profiler/scoped_tracker.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_toolbar_model_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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

ExtensionToolbarModel::ExtensionToolbarModel(Profile* profile,
                                             ExtensionPrefs* extension_prefs)
    : profile_(profile),
      extension_prefs_(extension_prefs),
      prefs_(profile_->GetPrefs()),
      extensions_initialized_(false),
      include_all_extensions_(
          FeatureSwitch::extension_action_redesign()->IsEnabled()),
      is_highlighting_(false),
      extension_action_observer_(this),
      extension_registry_observer_(this),
      weak_ptr_factory_(this) {
  ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE,
      base::Bind(&ExtensionToolbarModel::OnReady,
                 weak_ptr_factory_.GetWeakPtr()));
  visible_icon_count_ = prefs_->GetInteger(pref_names::kToolbarSize);

  // We only care about watching the prefs if not in incognito mode.
  if (!profile_->IsOffTheRecord()) {
    pref_change_registrar_.Init(prefs_);
    pref_change_callback_ =
        base::Bind(&ExtensionToolbarModel::OnExtensionToolbarPrefChange,
                   base::Unretained(this));
    pref_change_registrar_.Add(pref_names::kToolbar, pref_change_callback_);
  }
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

void ExtensionToolbarModel::MoveExtensionIcon(const std::string& id,
                                              size_t index) {
  ExtensionList::iterator pos = toolbar_items_.begin();
  while (pos != toolbar_items_.end() && (*pos)->id() != id)
    ++pos;
  if (pos == toolbar_items_.end()) {
    NOTREACHED();
    return;
  }
  scoped_refptr<const Extension> extension = *pos;
  toolbar_items_.erase(pos);

  ExtensionIdList::iterator pos_id = std::find(last_known_positions_.begin(),
                                               last_known_positions_.end(),
                                               id);
  if (pos_id != last_known_positions_.end())
    last_known_positions_.erase(pos_id);

  if (index < toolbar_items_.size()) {
    // If the index is not at the end, find the item currently at |index|, and
    // insert |extension| before it in both |toolbar_items_| and
    // |last_known_positions_|.
    ExtensionList::iterator iter = toolbar_items_.begin() + index;
    last_known_positions_.insert(std::find(last_known_positions_.begin(),
                                           last_known_positions_.end(),
                                           (*iter)->id()),
                                 id);
    toolbar_items_.insert(iter, extension);
  } else {
    // Otherwise, put |extension| at the end.
    DCHECK_EQ(toolbar_items_.size(), index);
    index = toolbar_items_.size();
    toolbar_items_.push_back(extension);
    last_known_positions_.push_back(id);
  }

  FOR_EACH_OBSERVER(
      Observer, observers_, ToolbarExtensionMoved(extension.get(), index));
  MaybeUpdateVisibilityPref(extension.get(), index);
  UpdatePrefs();
}

void ExtensionToolbarModel::SetVisibleIconCount(size_t count) {
  visible_icon_count_ = (count >= toolbar_items_.size()) ? -1 : count;

  // Only set the prefs if we're not in highlight mode and the profile is not
  // incognito. Highlight mode is designed to be a transitory state, and should
  //  not persist across browser restarts (though it may be re-entered), and we
  // don't store anything in incognito.
  if (!is_highlighting_ && !profile_->IsOffTheRecord()) {
    // Additionally, if we are using the new toolbar, any icons which are in the
    // overflow menu are considered "hidden". But it so happens that the times
    // we are likely to call SetVisibleIconCount() are also those when we are
    // in flux. So wait for things to cool down before setting the prefs.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionToolbarModel::MaybeUpdateVisibilityPrefs,
                   weak_ptr_factory_.GetWeakPtr()));
    prefs_->SetInteger(pref_names::kToolbarSize, visible_icon_count_);
  }

  FOR_EACH_OBSERVER(Observer, observers_, ToolbarVisibleCountChanged());
}

void ExtensionToolbarModel::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  const Extension* extension =
      ExtensionRegistry::Get(profile_)->enabled_extensions().GetByID(
          extension_action->extension_id());
  // Notify observers if the extension exists and is in the model.
  if (std::find(toolbar_items_.begin(), toolbar_items_.end(), extension) !=
          toolbar_items_.end()) {
    FOR_EACH_OBSERVER(
        Observer, observers_, ToolbarExtensionUpdated(extension));
  }
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

  AddExtension(extension);
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
  DCHECK_EQ(NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED, type);
  const Extension* extension =
      ExtensionRegistry::Get(profile_)->GetExtensionById(
          *content::Details<const std::string>(details).ptr(),
          ExtensionRegistry::EVERYTHING);

  bool visible = ExtensionActionAPI::GetBrowserActionVisibility(
                     extension_prefs_, extension->id());
  // Hiding works differently with the new and old toolbars.
  if (include_all_extensions_) {
    // It's possible that we haven't added this extension yet, if its
    // visibility was adjusted in the course of its initialization.
    if (std::find(toolbar_items_.begin(), toolbar_items_.end(), extension) ==
            toolbar_items_.end())
      return;

    int new_size = 0;
    int new_index = 0;
    if (visible) {
      // If this action used to be hidden, we can't possibly be showing all.
      DCHECK_LT(visible_icon_count(), toolbar_items_.size());
      // Grow the bar by one and move the extension to the end of the visibles.
      new_size = visible_icon_count() + 1;
      new_index = new_size - 1;
    } else {
      // If we're hiding one, we must be showing at least one.
      DCHECK_GE(visible_icon_count(), 0u);
      // Shrink the bar by one and move the extension to the beginning of the
      // overflow menu.
      new_size = visible_icon_count() - 1;
      new_index = new_size;
    }
    SetVisibleIconCount(new_size);
    MoveExtensionIcon(extension->id(), new_index);
  } else {  // Don't include all extensions.
    if (visible)
      AddExtension(extension);
    else
      RemoveExtension(extension);
  }
}

void ExtensionToolbarModel::OnReady() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  InitializeExtensionList();
  // Wait until the extension system is ready before observing any further
  // changes so that the toolbar buttons can be shown in their stable ordering
  // taken from prefs.
  extension_registry_observer_.Add(registry);
  extension_action_observer_.Add(ExtensionActionAPI::Get(profile_));
  registrar_.Add(
      this,
      extensions::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
      content::Source<ExtensionPrefs>(extension_prefs_));
}

size_t ExtensionToolbarModel::FindNewPositionFromLastKnownGood(
    const Extension* extension) {
  // See if we have last known good position for this extension.
  size_t new_index = 0;
  // Loop through the ID list of known positions, to count the number of visible
  // extension icons preceding |extension|.
  for (ExtensionIdList::const_iterator iter_id = last_known_positions_.begin();
       iter_id < last_known_positions_.end(); ++iter_id) {
    if ((*iter_id) == extension->id())
      return new_index;  // We've found the right position.
    // Found an id, need to see if it is visible.
    for (ExtensionList::const_iterator iter_ext = toolbar_items_.begin();
         iter_ext < toolbar_items_.end(); ++iter_ext) {
      if ((*iter_ext)->id() == (*iter_id)) {
        // This extension is visible, update the index value.
        ++new_index;
        break;
      }
    }
  }

  // Position not found.
  return toolbar_items_.size();
}

bool ExtensionToolbarModel::ShouldAddExtension(const Extension* extension) {
  // In incognito mode, don't add any extensions that aren't incognito-enabled.
  if (profile_->IsOffTheRecord() &&
      !util::IsIncognitoEnabled(extension->id(), profile_))
    return false;

  ExtensionActionManager* action_manager =
      ExtensionActionManager::Get(profile_);
  if (include_all_extensions_) {
    // In this case, we don't care about the browser action visibility, because
    // we want to show each extension regardless.
    // TODO(devlin): Extension actions which are not visible should be moved to
    // the overflow menu by default.
    return action_manager->GetExtensionAction(*extension) != NULL;
  }

  return action_manager->GetBrowserAction(*extension) &&
         ExtensionActionAPI::GetBrowserActionVisibility(
             extension_prefs_, extension->id());
}

void ExtensionToolbarModel::AddExtension(const Extension* extension) {
  // We only use AddExtension() once the system is initialized.
  DCHECK(extensions_initialized_);
  if (!ShouldAddExtension(extension))
    return;

  // See if we have a last known good position for this extension.
  bool is_new_extension =
      std::find(last_known_positions_.begin(),
                last_known_positions_.end(),
                extension->id()) == last_known_positions_.end();

  // New extensions go at the right (end) of the visible extensions. Other
  // extensions go at their previous position.
  size_t new_index = 0;
  if (is_new_extension) {
    new_index = visible_icon_count();
    // For the last-known position, we use the index of the extension that is
    // just before this extension, plus one. (Note that this isn't the same
    // as new_index + 1, because last_known_positions_ can include disabled
    // extensions.)
    int new_last_known_index =
        new_index == 0 ? 0 :
        std::find(last_known_positions_.begin(),
                  last_known_positions_.end(),
                  toolbar_items_[new_index - 1]->id()) -
            last_known_positions_.begin() + 1;
    // In theory, the extension before this one should always
    // be in last known positions, but if something funny happened with prefs,
    // make sure we handle it.
    // TODO(devlin): Track down these cases so we can CHECK this.
    new_last_known_index =
        std::min<int>(new_last_known_index, last_known_positions_.size());
    last_known_positions_.insert(
        last_known_positions_.begin() + new_last_known_index, extension->id());
    UpdatePrefs();
  } else {
    new_index = FindNewPositionFromLastKnownGood(extension);
  }

  toolbar_items_.insert(toolbar_items_.begin() + new_index, extension);

  // If we're currently highlighting, then even though we add a browser action
  // to the full list (|toolbar_items_|, there won't be another *visible*
  // browser action, which was what the observers care about.
  if (!is_highlighting_) {
    FOR_EACH_OBSERVER(
        Observer, observers_, ToolbarExtensionAdded(extension, new_index));

    int visible_count_delta = 0;
    if (is_new_extension && !all_icons_visible()) {
      // If this is a new extension (and not all extensions are visible), we
      // expand the toolbar out so that the new one can be seen.
      visible_count_delta = 1;
    } else if (profile_->IsOffTheRecord()) {
      // If this is an incognito profile, we also have to check to make sure the
      // overflow matches the main bar's status.
      ExtensionToolbarModel* main_model =
          ExtensionToolbarModel::Get(profile_->GetOriginalProfile());
      // Find what the index will be in the main bar. Because Observer calls are
      // nondeterministic, we can't just assume the main bar will have the
      // extension and look it up.
      size_t main_index =
          main_model->FindNewPositionFromLastKnownGood(extension);
      bool visible = main_index < main_model->visible_icon_count();
      // We may need to adjust the visible count if the incognito bar isn't
      // showing all icons and this one is visible, or if it is showing all
      // icons and this is hidden.
      if (visible && !all_icons_visible())
        visible_count_delta = 1;
      else if (!visible && all_icons_visible())
        visible_count_delta = -1;
    }

    if (visible_count_delta)
      SetVisibleIconCount(visible_icon_count() + visible_count_delta);
  }

  MaybeUpdateVisibilityPref(extension, new_index);
}

void ExtensionToolbarModel::RemoveExtension(const Extension* extension) {
  ExtensionList::iterator pos =
      std::find(toolbar_items_.begin(), toolbar_items_.end(), extension);
  if (pos == toolbar_items_.end())
    return;

  // If our visible count is set to the current size, we need to decrement it.
  if (visible_icon_count_ == static_cast<int>(toolbar_items_.size()))
    SetVisibleIconCount(toolbar_items_.size() - 1);

  toolbar_items_.erase(pos);

  // If we're in highlight mode, we also have to remove the extension from
  // the highlighted list.
  if (is_highlighting_) {
    pos = std::find(highlighted_items_.begin(),
                    highlighted_items_.end(),
                    extension);
    if (pos != highlighted_items_.end()) {
      highlighted_items_.erase(pos);
      FOR_EACH_OBSERVER(
          Observer, observers_, ToolbarExtensionRemoved(extension));
      // If the highlighted list is now empty, we stop highlighting.
      if (highlighted_items_.empty())
        StopHighlighting();
    }
  } else {
    FOR_EACH_OBSERVER(Observer, observers_, ToolbarExtensionRemoved(extension));
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
void ExtensionToolbarModel::InitializeExtensionList() {
  DCHECK(toolbar_items_.empty());  // We shouldn't have any items yet.

  // TODO(robliao): Remove ScopedTracker below once crbug.com/463337 is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "463337 ExtensionToolbarModel::InitializeExtensionList1"));
  last_known_positions_ = extension_prefs_->GetToolbarOrder();
  if (profile_->IsOffTheRecord()) {
    // TODO(robliao): Remove ScopedTracker below once crbug.com/463337 is fixed.
    tracked_objects::ScopedTracker tracking_profile2(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "463337 ExtensionToolbarModel::InitializeExtensionList2"));
    IncognitoPopulate();
  } else {
    // TODO(robliao): Remove ScopedTracker below once crbug.com/463337 is fixed.
    tracked_objects::ScopedTracker tracking_profile3(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "463337 ExtensionToolbarModel::InitializeExtensionList3"));
    Populate(&last_known_positions_);
  }

  extensions_initialized_ = true;

  // TODO(robliao): Remove ScopedTracker below once crbug.com/463337 is fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "463337 ExtensionToolbarModel::InitializeExtensionList4"));
  MaybeUpdateVisibilityPrefs();

  // TODO(robliao): Remove ScopedTracker below once crbug.com/463337 is fixed.
  tracked_objects::ScopedTracker tracking_profile5(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "463337 ExtensionToolbarModel::InitializeExtensionList5"));
  FOR_EACH_OBSERVER(Observer, observers_, OnToolbarModelInitialized());
}

void ExtensionToolbarModel::Populate(ExtensionIdList* positions) {
  DCHECK(!profile_->IsOffTheRecord());
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  // Items that have explicit positions.
  ExtensionList sorted(positions->size(), NULL);
  // The items that don't have explicit positions.
  ExtensionList unsorted;

  // Create the lists.
  int hidden = 0;
  for (const scoped_refptr<const Extension>& extension : extensions) {
    if (!ShouldAddExtension(extension.get())) {
      if (!ExtensionActionAPI::GetBrowserActionVisibility(extension_prefs_,
                                                          extension->id()))
        ++hidden;
      continue;
    }

    ExtensionIdList::const_iterator pos =
        std::find(positions->begin(), positions->end(), extension->id());
    if (pos != positions->end()) {
      sorted[pos - positions->begin()] = extension;
    } else {
      // Unknown extension - push it to the back of unsorted, and add it to the
      // list of ids at the end.
      unsorted.push_back(extension);
      positions->push_back(extension->id());
    }
  }

  // Merge the lists.
  sorted.insert(sorted.end(), unsorted.begin(), unsorted.end());
  toolbar_items_.reserve(sorted.size());

  for (const scoped_refptr<const Extension>& extension : sorted) {
    // It's possible for the extension order to contain items that aren't
    // actually loaded on this machine.  For example, when extension sync is on,
    // we sync the extension order as-is but double-check with the user before
    // syncing NPAPI-containing extensions, so if one of those is not actually
    // synced, we'll get a NULL in the list.  This sort of case can also happen
    // if some error prevents an extension from loading.
    if (extension.get()) {
      // We don't notify observers of the added extension yet. Rather, observers
      // should wait for the "OnToolbarModelInitialized" notification, and then
      // bulk-update. (This saves a lot of bouncing-back-and-forth here, and
      // allows observers to ensure that the extension system is always
      // initialized before using the extensions).
      toolbar_items_.push_back(extension);
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

void ExtensionToolbarModel::IncognitoPopulate() {
  DCHECK(profile_->IsOffTheRecord());
  const ExtensionToolbarModel* original_model =
      ExtensionToolbarModel::Get(profile_->GetOriginalProfile());

  // Find the absolute value of the original model's count.
  int original_visible = original_model->visible_icon_count();

  // In incognito mode, we show only those extensions that are
  // incognito-enabled. Further, any actions that were overflowed in regular
  // mode are still overflowed. Order is the same as in regular mode.
  visible_icon_count_ = 0;
  for (ExtensionList::const_iterator iter =
           original_model->toolbar_items_.begin();
       iter != original_model->toolbar_items_.end(); ++iter) {
    if (ShouldAddExtension(iter->get())) {
      toolbar_items_.push_back(*iter);
      if (iter - original_model->toolbar_items_.begin() < original_visible)
        ++visible_icon_count_;
    }
  }
}

void ExtensionToolbarModel::UpdatePrefs() {
  if (!extension_prefs_ || profile_->IsOffTheRecord())
    return;

  // Don't observe change caused by self.
  pref_change_registrar_.Remove(pref_names::kToolbar);
  extension_prefs_->SetToolbarOrder(last_known_positions_);
  pref_change_registrar_.Add(pref_names::kToolbar, pref_change_callback_);
}

void ExtensionToolbarModel::MaybeUpdateVisibilityPref(
    const Extension* extension, size_t index) {
  // We only update the visibility pref for hidden/not hidden based on the
  // overflow menu with the new toolbar design.
  if (include_all_extensions_ && !profile_->IsOffTheRecord()) {
    bool visible = index < visible_icon_count();
    if (visible != ExtensionActionAPI::GetBrowserActionVisibility(
                       extension_prefs_, extension->id())) {
      // Don't observe changes caused by ourselves.
      bool was_registered = false;
      if (registrar_.IsRegistered(
              this,
              NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
              content::Source<ExtensionPrefs>(extension_prefs_))) {
        was_registered = true;
        registrar_.Remove(
            this,
            NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
            content::Source<ExtensionPrefs>(extension_prefs_));
      }
      ExtensionActionAPI::SetBrowserActionVisibility(
          extension_prefs_, extension->id(), visible);
      if (was_registered) {
        registrar_.Add(this,
                       NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
                       content::Source<ExtensionPrefs>(extension_prefs_));
      }
    }
  }
}

void ExtensionToolbarModel::MaybeUpdateVisibilityPrefs() {
  for (size_t i = 0u; i < toolbar_items_.size(); ++i)
    MaybeUpdateVisibilityPref(toolbar_items_[i].get(), i);
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

  // Clear out the old...
  while (!toolbar_items_.empty()) {
    scoped_refptr<const Extension> extension = toolbar_items_.back();
    toolbar_items_.pop_back();
    FOR_EACH_OBSERVER(
        Observer, observers_, ToolbarExtensionRemoved(extension.get()));
  }
  DCHECK(toolbar_items_.empty());

  // ...Add the new...
  Populate(&last_known_positions_);

  // ...And notify.
  for (size_t i = 0; i < toolbar_items().size(); ++i) {
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      ToolbarExtensionAdded(toolbar_items()[i].get(), i));
  }

  if (last_known_positions_.size() > pref_position_size) {
    // Need to update pref because we have extra icons. But can't call
    // UpdatePrefs() directly within observation closure.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionToolbarModel::UpdatePrefs,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

int ExtensionToolbarModel::GetIndexForId(const std::string& id) const {
  for (size_t i = 0; i < toolbar_items().size(); ++i) {
    if (toolbar_items()[i]->id() == id)
      return i;
  }
  return -1;
}

bool ExtensionToolbarModel::ShowExtensionActionPopup(
    const Extension* extension,
    Browser* browser,
    bool grant_active_tab) {
  ObserverListBase<Observer>::Iterator it(observers_);
  Observer* obs = NULL;
  // Look for the Observer associated with the browser.
  // This would be cleaner if we had an abstract class for the Toolbar UI
  // (like we do for LocationBar), but sadly, we don't.
  while ((obs = it.GetNext()) != NULL) {
    if (obs->GetBrowser() == browser)
      return obs->ShowExtensionActionPopup(extension, grant_active_tab);
  }
  return false;
}

void ExtensionToolbarModel::EnsureVisibility(
    const ExtensionIdList& extension_ids) {
  if (all_icons_visible())
    return;  // Already showing all.

  // Otherwise, make sure we have enough room to show all the extensions
  // requested.
  if (visible_icon_count() < extension_ids.size())
    SetVisibleIconCount(extension_ids.size());

  if (all_icons_visible())
    return;  // May have been set to max by SetVisibleIconCount.

  // Guillotine's Delight: Move an orange noble to the front of the line.
  for (ExtensionIdList::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    for (ExtensionList::const_iterator extension = toolbar_items_.begin();
         extension != toolbar_items_.end(); ++extension) {
      if ((*extension)->id() == (*it)) {
        if (extension - toolbar_items_.begin() >=
                static_cast<int>(visible_icon_count()))
          MoveExtensionIcon((*extension)->id(), 0);
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
    if (visible_icon_count() < extension_ids.size())
      SetVisibleIconCount(extension_ids.size());

    FOR_EACH_OBSERVER(Observer, observers_, ToolbarHighlightModeChanged(true));
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
    if (old_visible_icon_count_ != visible_icon_count_)
      SetVisibleIconCount(old_visible_icon_count_);
    FOR_EACH_OBSERVER(Observer, observers_, ToolbarHighlightModeChanged(false));
  }
}

}  // namespace extensions
