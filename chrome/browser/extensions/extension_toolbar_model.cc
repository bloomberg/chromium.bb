// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_toolbar_model.h"

#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

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
      content::Source<ExtensionPrefs>(service_->extension_prefs()));

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
  ExtensionList::iterator pos = std::find(begin(), end(), extension);
  if (pos == end()) {
    NOTREACHED();
    return;
  }
  toolitems_.erase(pos);

  int i = 0;
  bool inserted = false;
  for (ExtensionList::iterator iter = begin(); iter != end(); ++iter, ++i) {
    if (i == index) {
      toolitems_.insert(iter, make_scoped_refptr(extension));
      inserted = true;
      break;
    }
  }

  if (!inserted) {
    DCHECK_EQ(index, static_cast<int>(toolitems_.size()));
    index = toolitems_.size();

    toolitems_.push_back(make_scoped_refptr(extension));
  }

  FOR_EACH_OBSERVER(Observer, observers_, BrowserActionMoved(extension, index));

  UpdatePrefs();
}

void ExtensionToolbarModel::ExecuteBrowserAction(
    const std::string& extension_id, Browser* browser) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    BrowserActionExecuted(extension_id, browser));
}

void ExtensionToolbarModel::SetVisibleIconCount(int count) {
  visible_icon_count_ = count == static_cast<int>(size()) ? -1 : count;
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
    extension = content::Details<UnloadedExtensionInfo>(details)->extension;
  } else {
    extension = content::Details<const Extension>(details).ptr();
  }
  if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
    // We don't want to add the same extension twice. It may have already been
    // added by EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED below, if the user
    // hides the browser action and then disables and enables the extension.
    for (size_t i = 0; i < toolitems_.size(); i++) {
      if (toolitems_[i].get() == extension)
        return;  // Already exists.
    }
    if (service_->extension_prefs()->GetBrowserActionVisibility(extension))
      AddExtension(extension);
  } else if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    RemoveExtension(extension);
  } else if (
        type ==
        chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED) {
    if (service_->extension_prefs()->GetBrowserActionVisibility(extension))
      AddExtension(extension);
    else
      RemoveExtension(extension);
  } else {
    NOTREACHED() << "Received unexpected notification";
  }
}

void ExtensionToolbarModel::AddExtension(const Extension* extension) {
  // We only care about extensions with browser actions.
  if (!extension->browser_action())
    return;

  if (extension->id() == last_extension_removed_ &&
      last_extension_removed_index_ < toolitems_.size()) {
    toolitems_.insert(begin() + last_extension_removed_index_,
                      make_scoped_refptr(extension));
    FOR_EACH_OBSERVER(Observer, observers_,
        BrowserActionAdded(extension, last_extension_removed_index_));
  } else {
    toolitems_.push_back(make_scoped_refptr(extension));
    FOR_EACH_OBSERVER(Observer, observers_,
                      BrowserActionAdded(extension, toolitems_.size() - 1));
  }

  last_extension_removed_ = "";
  last_extension_removed_index_ = -1;

  UpdatePrefs();
}

void ExtensionToolbarModel::RemoveExtension(const Extension* extension) {
  ExtensionList::iterator pos = std::find(begin(), end(), extension);
  if (pos == end()) {
    return;
  }

  last_extension_removed_ = extension->id();
  last_extension_removed_index_ = pos - begin();

  toolitems_.erase(pos);
  FOR_EACH_OBSERVER(Observer, observers_,
                    BrowserActionRemoved(extension));

  UpdatePrefs();
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

  std::vector<std::string> pref_order = service_->extension_prefs()->
      GetToolbarOrder();
  // Items that have a pref for their position.
  ExtensionList sorted;
  sorted.resize(pref_order.size(), NULL);
  // The items that don't have a pref for their position.
  ExtensionList unsorted;

  // Create the lists.
  for (ExtensionSet::const_iterator it = service_->extensions()->begin();
       it != service_->extensions()->end(); ++it) {
    const Extension* extension = *it;
    if (!extension->browser_action())
      continue;
    if (!service_->extension_prefs()->GetBrowserActionVisibility(extension))
      continue;

    std::vector<std::string>::iterator pos =
        std::find(pref_order.begin(), pref_order.end(), extension->id());
    if (pos != pref_order.end()) {
      int index = std::distance(pref_order.begin(), pos);
      sorted[index] = extension;
    } else {
      unsorted.push_back(make_scoped_refptr(extension));
    }
  }

  // Merge the lists.
  toolitems_.reserve(sorted.size() + unsorted.size());
  for (ExtensionList::iterator iter = sorted.begin();
       iter != sorted.end(); ++iter) {
    if (*iter != NULL)
      toolitems_.push_back(*iter);
  }
  toolitems_.insert(toolitems_.end(), unsorted.begin(), unsorted.end());

  // Inform observers.
  for (size_t i = 0; i < toolitems_.size(); i++) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      BrowserActionAdded(toolitems_[i], i));
  }

  UpdatePrefs();

  extensions_initialized_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, ModelLoaded());
}

void ExtensionToolbarModel::UpdatePrefs() {
  if (!service_->extension_prefs())
    return;

  std::vector<std::string> ids;
  ids.reserve(toolitems_.size());
  for (ExtensionList::iterator iter = begin(); iter != end(); ++iter)
    ids.push_back((*iter)->id());
  service_->extension_prefs()->SetToolbarOrder(ids);
}

const Extension* ExtensionToolbarModel::GetExtensionByIndex(int index) const {
  return toolitems_[index];
}

int ExtensionToolbarModel::IncognitoIndexToOriginal(int incognito_index) {
  int original_index = 0, i = 0;
  for (ExtensionList::iterator iter = begin(); iter != end();
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
  for (ExtensionList::iterator iter = begin(); iter != end();
       ++iter, ++i) {
    if (original_index == i)
      break;
    if (service_->IsIncognitoEnabled((*iter)->id()))
      ++incognito_index;
  }
  return incognito_index;
}
