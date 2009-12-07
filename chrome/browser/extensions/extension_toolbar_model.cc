// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_toolbar_model.h"

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

void ExtensionToolbarModel::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  if (type == NotificationType::EXTENSIONS_READY) {
    for (size_t i = 0; i < service_->extensions()->size(); ++i) {
      Extension* extension = service_->GetExtensionById(
          service_->extensions()->at(i)->id(), false);
      AddExtension(extension);
    }
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
}

void ExtensionToolbarModel::RemoveExtension(Extension* extension) {
  ExtensionList::iterator pos = std::find(begin(), end(), extension);
  if (pos != end()) {
    toolitems_.erase(pos);
    FOR_EACH_OBSERVER(Observer, observers_,
                      BrowserActionRemoved(extension));
  }
}
