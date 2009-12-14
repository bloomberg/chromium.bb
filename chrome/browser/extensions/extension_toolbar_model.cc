// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_toolbar_model.h"

#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"

ExtensionToolbarModel::ExtensionToolbarModel(ExtensionsService* service)
    : service_(service) {
  DCHECK(service_);

  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 Source<Profile>(service_->profile()));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(service_->profile()));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
                 Source<Profile>(service_->profile()));
  registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                 Source<Profile>(service_->profile()));
}

ExtensionToolbarModel::~ExtensionToolbarModel() {
}

void ExtensionToolbarModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionToolbarModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionToolbarModel::MoveBrowserAction(Extension* extension,
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
      toolitems_.insert(iter, extension);
      inserted = true;
      break;
    }
  }

  if (!inserted) {
    DCHECK_EQ(index, static_cast<int>(toolitems_.size()));
    index = toolitems_.size();

    toolitems_.push_back(extension);
  }

  FOR_EACH_OBSERVER(Observer, observers_, BrowserActionMoved(extension, index));

  UpdatePrefs();
}

void ExtensionToolbarModel::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  if (type == NotificationType::EXTENSIONS_READY) {
    InitializeExtensionList();
    return;
  }

  if (!service_->is_ready())
    return;

  Extension* extension = Details<Extension>(details).ptr();
  if (type == NotificationType::EXTENSION_LOADED) {
    AddExtension(extension);
  } else if (type == NotificationType::EXTENSION_UNLOADED ||
             type == NotificationType::EXTENSION_UNLOADED_DISABLED) {
    RemoveExtension(extension);
  } else {
    NOTREACHED() << "Received unexpected notification";
  }
}

void ExtensionToolbarModel::AddExtension(Extension* extension) {
  // We only care about extensions with browser actions.
  if (!extension->browser_action())
    return;

  toolitems_.push_back(extension);
  FOR_EACH_OBSERVER(Observer, observers_,
                    BrowserActionAdded(extension, toolitems_.size() - 1));

  UpdatePrefs();
}

void ExtensionToolbarModel::RemoveExtension(Extension* extension) {
  ExtensionList::iterator pos = std::find(begin(), end(), extension);
  if (pos == end()) {
    return;
  }

  toolitems_.erase(pos);
  FOR_EACH_OBSERVER(Observer, observers_,
                    BrowserActionRemoved(extension));

  UpdatePrefs();
}

// Combine the currently enabled extensions that have browser actions (which
// we get from the ExtensionsService) with the ordering we get from the
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
  for (size_t i = 0; i < service_->extensions()->size(); ++i) {
    Extension* extension = service_->extensions()->at(i);
    if (!extension->browser_action())
      continue;

    std::vector<std::string>::iterator pos =
        std::find(pref_order.begin(), pref_order.end(), extension->id());
    if (pos != pref_order.end()) {
      int index = std::distance(pref_order.begin(), pos);
      sorted[index] = extension;
    } else {
      unsorted.push_back(extension);
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
